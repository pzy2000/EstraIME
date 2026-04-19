use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ImeConfig {
    pub ime: ImeSection,
    pub llm: LlmSection,
    pub privacy: PrivacySection,
    pub diagnostics: DiagnosticsSection,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ImeSection {
    pub mode_default: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LlmSection {
    pub enabled: bool,
    pub provider: String,
    pub local: LocalSection,
    pub trigger: TriggerSection,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LocalSection {
    pub model_id: String,
    pub endpoint: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TriggerSection {
    pub pause_ms: u64,
    pub min_chars: usize,
    pub min_context_chars: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PrivacySection {
    pub mode: String,
    pub cloud_opt_in: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DiagnosticsSection {
    pub log_level: String,
}

impl Default for ImeConfig {
    fn default() -> Self {
        Self {
            ime: ImeSection {
                mode_default: "chinese".into(),
            },
            llm: LlmSection {
                enabled: true,
                provider: "mock".into(),
                local: LocalSection {
                    model_id: "qwen3-1.7b-q4".into(),
                    endpoint: "http://127.0.0.1:8080/v1/chat/completions".into(),
                },
                trigger: TriggerSection {
                    pause_ms: 180,
                    min_chars: 4,
                    min_context_chars: 8,
                },
            },
            privacy: PrivacySection {
                mode: "strict".into(),
                cloud_opt_in: false,
            },
            diagnostics: DiagnosticsSection {
                log_level: "info".into(),
            },
        }
    }
}

pub fn default_config_path() -> PathBuf {
    let local_app_data = std::env::var("LOCALAPPDATA")
        .or_else(|_| std::env::var("HOME"))
        .unwrap_or_else(|_| ".".into());
    PathBuf::from(local_app_data).join("EstraIME").join("ime-config.json")
}

pub fn load_or_default() -> anyhow::Result<ImeConfig> {
    let path = default_config_path();
    if !path.exists() {
        return Ok(ImeConfig::default());
    }

    let content = fs::read_to_string(path)?;
    Ok(serde_json::from_str(&content)?)
}

pub fn save(config: &ImeConfig) -> anyhow::Result<()> {
    let path = default_config_path();
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    fs::write(path, serde_json::to_string_pretty(config)?)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_config_is_local_first() {
        let config = ImeConfig::default();
        assert!(config.llm.enabled);
        assert_eq!(config.llm.provider, "mock");
    }
}
