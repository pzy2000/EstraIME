use async_trait::async_trait;
use provider_api::{AutocompleteRequest, CompletionProvider, ProviderHealth, Suggestion};

pub struct CloudStubProvider;

#[async_trait]
impl CompletionProvider for CloudStubProvider {
    async fn complete(&self, _request: &AutocompleteRequest) -> anyhow::Result<Vec<Suggestion>> {
        anyhow::bail!("cloud provider is disabled by default in alpha")
    }

    async fn health(&self) -> ProviderHealth {
        ProviderHealth {
            backend: "cloud-disabled".into(),
            ready: false,
            details: "cloud provider is intentionally disabled".into(),
        }
    }
}
