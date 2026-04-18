use async_trait::async_trait;
use provider_api::{AutocompleteRequest, CompletionProvider, ProviderHealth, Suggestion};

pub struct MockProvider;

#[async_trait]
impl CompletionProvider for MockProvider {
    async fn complete(&self, request: &AutocompleteRequest) -> anyhow::Result<Vec<Suggestion>> {
        if request.secure_input || request.composition_text.is_empty() {
            return Ok(Vec::new());
        }

        let lower = request.composition_text.to_ascii_lowercase();
        let suggestions = if lower.contains("nihao") {
            vec![
                Suggestion {
                    text: "你好世界".into(),
                    score: 0.88,
                    source: "llm".into(),
                    annotation: "mock".into(),
                },
                Suggestion {
                    text: "你好，今天".into(),
                    score: 0.77,
                    source: "llm".into(),
                    annotation: "mock".into(),
                },
            ]
        } else if lower.contains("zhongwen") {
            vec![Suggestion {
                text: "中文输入法".into(),
                score: 0.81,
                source: "llm".into(),
                annotation: "mock".into(),
            }]
        } else {
            vec![Suggestion {
                text: format!("{}{}", request.composition_text, "完成"),
                score: 0.72,
                source: "llm".into(),
                annotation: "mock".into(),
            }]
        };

        Ok(suggestions)
    }

    async fn health(&self) -> ProviderHealth {
        ProviderHealth {
            backend: "mock".into(),
            ready: true,
            details: "mock provider active".into(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn returns_mock_suggestions() {
        let provider = MockProvider;
        let suggestions = provider
            .complete(&AutocompleteRequest {
                session_id: "s".into(),
                request_id: "r".into(),
                generation_id: 1,
                context_before: String::new(),
                composition_text: "nihao".into(),
                supports_ui: true,
                secure_input: false,
            })
            .await
            .unwrap();

        assert!(!suggestions.is_empty());
    }
}
