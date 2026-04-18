use async_trait::async_trait;
use provider_api::{AutocompleteRequest, CompletionProvider, ProviderHealth, Suggestion};
use serde::{Deserialize, Serialize};

pub struct LlamaCppProvider {
    endpoint: String,
    client: reqwest::Client,
    model_id: String,
}

impl LlamaCppProvider {
    pub fn new(endpoint: impl Into<String>, model_id: impl Into<String>) -> Self {
        Self {
            endpoint: endpoint.into(),
            client: reqwest::Client::new(),
            model_id: model_id.into(),
        }
    }
}

#[derive(Serialize)]
struct ChatRequest {
    model: String,
    messages: Vec<ChatMessage>,
    temperature: f32,
    max_tokens: u32,
}

#[derive(Serialize, Deserialize)]
struct ChatMessage {
    role: String,
    content: String,
}

#[derive(Deserialize)]
struct ChatResponse {
    choices: Vec<Choice>,
}

#[derive(Deserialize)]
struct Choice {
    message: ChatMessage,
}

#[async_trait]
impl CompletionProvider for LlamaCppProvider {
    async fn complete(&self, request: &AutocompleteRequest) -> anyhow::Result<Vec<Suggestion>> {
        if request.secure_input {
            return Ok(Vec::new());
        }

        let prompt = format!(
            "你是中文输入法补全器。只返回短候选，每行一个，不要解释。\n上文:{}\n当前拼音:{}",
            request.context_before, request.composition_text
        );

        let response = self
            .client
            .post(&self.endpoint)
            .json(&ChatRequest {
                model: self.model_id.clone(),
                messages: vec![ChatMessage {
                    role: "user".into(),
                    content: prompt,
                }],
                temperature: 0.2,
                max_tokens: 32,
            })
            .send()
            .await?
            .error_for_status()?
            .json::<ChatResponse>()
            .await?;

        let content = response
            .choices
            .into_iter()
            .next()
            .map(|choice| choice.message.content)
            .unwrap_or_default();

        let suggestions = content
            .lines()
            .map(str::trim)
            .filter(|line| !line.is_empty())
            .take(3)
            .enumerate()
            .map(|(index, text)| Suggestion {
                text: text.trim_start_matches(|c: char| c.is_ascii_digit() || c == '.' || c == '-' || c.is_whitespace()).to_string(),
                score: 0.90 - index as f64 * 0.05,
                source: "llm".into(),
                annotation: "llama.cpp".into(),
            })
            .collect();

        Ok(suggestions)
    }

    async fn health(&self) -> ProviderHealth {
        ProviderHealth {
            backend: "llama.cpp".into(),
            ready: true,
            details: self.endpoint.clone(),
        }
    }
}
