use anyhow::Context;
use config_store::{load_or_default, ImeConfig};
use provider_api::{AutocompleteRequest, CompletionProvider};
use provider_cloud_stub::CloudStubProvider;
use provider_llama_cpp::LlamaCppProvider;
use provider_mock::MockProvider;
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::windows::named_pipe::ServerOptions;
use tracing::{error, info, warn};

const PIPE_NAME: &str = r"\\.\pipe\LOCAL\EstraIME.Sidecar";

#[derive(Debug, Deserialize)]
struct IpcEnvelope {
    method: String,
    #[serde(default)]
    session_id: String,
    #[serde(default)]
    request_id: String,
    #[serde(default)]
    generation_id: u64,
    #[serde(default)]
    context_before: String,
    #[serde(default)]
    composition_text: String,
    #[serde(default)]
    caret_caps: CaretCaps,
}

#[derive(Debug, Default, Deserialize)]
struct CaretCaps {
    #[serde(default)]
    supports_ui: bool,
    #[serde(default)]
    secure_input: bool,
}

#[derive(Serialize)]
struct SuggestionWire {
    text: String,
    score: f64,
    source: String,
    annotation: String,
}

#[derive(Serialize)]
struct AutocompleteResponseWire {
    method: &'static str,
    request_id: String,
    generation_id: u64,
    suggestions: Vec<SuggestionWire>,
}

#[derive(Serialize)]
struct HealthResponseWire {
    method: &'static str,
    backend: String,
    ready: bool,
    details: String,
}

#[derive(Serialize)]
struct AckWire {
    method: &'static str,
    ok: bool,
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt().with_env_filter("info").init();

    let config = load_or_default().context("failed to load config")?;
    info!("sidecar starting with provider={}", config.llm.provider);

    let mut listener = ServerOptions::new()
        .first_pipe_instance(true)
        .create(PIPE_NAME)
        .with_context(|| format!("failed to create named pipe {}", PIPE_NAME))?;

    loop {
        listener.connect().await?;

        let next_listener = ServerOptions::new()
            .create(PIPE_NAME)
            .with_context(|| format!("failed to create named pipe {}", PIPE_NAME))?;
        let server = std::mem::replace(&mut listener, next_listener);
        tokio::spawn(async move {
            if let Err(error) = handle_client(server).await {
                warn!("pipe client handling failed: {}", error);
            }
        });
    }
}

fn provider_from_config(config: &ImeConfig) -> Arc<dyn CompletionProvider> {
    match config.llm.provider.as_str() {
        "local" => Arc::new(LlamaCppProvider::new(
            config.llm.local.endpoint.clone(),
            config.llm.local.model_id.clone(),
        )),
        "cloud" => Arc::new(CloudStubProvider),
        _ => Arc::new(MockProvider),
    }
}

async fn handle_client(mut server: tokio::net::windows::named_pipe::NamedPipeServer) -> anyhow::Result<()> {
    let mut len_buf = [0u8; 4];
    if server.read_exact(&mut len_buf).await.is_err() {
        return Ok(());
    }
    let payload_len = u32::from_le_bytes(len_buf) as usize;
    let mut payload = vec![0u8; payload_len];
    server.read_exact(&mut payload).await?;

    let envelope: IpcEnvelope = match serde_json::from_slice(&payload) {
        Ok(value) => value,
        Err(error) => {
            warn!("invalid ipc payload: {}", error);
            return Ok(());
        }
    };

    let config = load_or_default().unwrap_or_default();
    let provider = provider_from_config(&config);

    let response = match envelope.method.as_str() {
        "autocomplete.request" => {
            let request = AutocompleteRequest {
                session_id: envelope.session_id,
                request_id: envelope.request_id.clone(),
                generation_id: envelope.generation_id,
                context_before: envelope.context_before,
                composition_text: envelope.composition_text,
                supports_ui: envelope.caret_caps.supports_ui,
                secure_input: envelope.caret_caps.secure_input,
            };

            let suggestions = provider.complete(&request).await.unwrap_or_else(|error| {
                error!("provider completion failed: {}", error);
                Vec::new()
            });

            serde_json::to_vec(&AutocompleteResponseWire {
                method: "autocomplete.response",
                request_id: request.request_id,
                generation_id: request.generation_id,
                suggestions: suggestions
                    .into_iter()
                    .map(|item| SuggestionWire {
                        text: item.text,
                        score: item.score,
                        source: item.source,
                        annotation: item.annotation,
                    })
                    .collect(),
            })?
        }
        "health.get" => {
            let health = provider.health().await;
            serde_json::to_vec(&HealthResponseWire {
                method: "health.response",
                backend: health.backend,
                ready: health.ready,
                details: health.details,
            })?
        }
        "autocomplete.cancel" | "learn.commit" => serde_json::to_vec(&AckWire {
            method: "ack",
            ok: true,
        })?,
        other => {
            warn!("unsupported method: {}", other);
            serde_json::to_vec(&AckWire {
                method: "ack",
                ok: false,
            })?
        }
    };

    server
        .write_all(&(response.len() as u32).to_le_bytes())
        .await?;
    server.write_all(&response).await?;
    server.flush().await?;
    Ok(())
}
