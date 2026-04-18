use async_trait::async_trait;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AutocompleteRequest {
    pub session_id: String,
    pub request_id: String,
    pub generation_id: u64,
    pub context_before: String,
    pub composition_text: String,
    pub supports_ui: bool,
    pub secure_input: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Suggestion {
    pub text: String,
    pub score: f64,
    pub source: String,
    pub annotation: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProviderHealth {
    pub backend: String,
    pub ready: bool,
    pub details: String,
}

#[async_trait]
pub trait CompletionProvider: Send + Sync {
    async fn complete(&self, request: &AutocompleteRequest) -> anyhow::Result<Vec<Suggestion>>;
    async fn health(&self) -> ProviderHealth;
}
