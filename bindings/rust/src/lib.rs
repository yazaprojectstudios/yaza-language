//! YazaBase Rust Client — High performance embedded database driver.

use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Debug, thiserror::Error)]
pub enum YazaBaseError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("CLI execution failed: {0}")]
    Execution(String),
    #[error("CSV parsing error: {0}")]
    Csv(String),
}

pub struct YazaBase {
    dir_path: PathBuf,
    db_name: String,
    cli_path: PathBuf,
}

impl YazaBase {
    pub fn open<P: AsRef<Path>>(dir_path: P, db_name: &str) -> Result<Self, YazaBaseError> {
        let dir = dir_path.as_ref().to_path_buf();
        fs::create_dir_all(&dir)?;

        let cli = PathBuf::from("yazabase");
        let db = Self {
            dir_path: dir.clone(),
            db_name: db_name.to_string(),
            cli_path: cli,
        };

        let db_file = dir.join(format!("{}.ydb", db_name));
        if !db_file.exists() {
            let output = Command::new(&db.cli_path)
                .args(["init", dir.to_str().unwrap(), db_name])
                .output()?;
            if !output.status.success() {
                return Err(YazaBaseError::Execution(
                    String::from_utf8_lossy(&output.stderr).to_string(),
                ));
            }
        }
        Ok(db)
    }

    pub fn query(&self, sql: &str) -> Result<Vec<HashMap<String, Option<String>>>, YazaBaseError> {
        let output = Command::new(&self.cli_path)
            .args([
                "sql",
                "--csv",
                self.dir_path.to_str().unwrap(),
                &self.db_name,
                sql,
            ])
            .output()?;

        if !output.status.success() {
            return Err(YazaBaseError::Execution(
                String::from_utf8_lossy(&output.stderr).to_string(),
            ));
        }

        let text = String::from_utf8_lossy(&output.stdout);
        let mut lines = text.lines();
        let headers: Vec<String> = match lines.next() {
            Some(h) => h.split(',').map(|s| s.to_string()).collect(),
            None => return Ok(Vec::new()),
        };

        let mut results = Vec::new();
        for line in lines {
            let cols: Vec<&str> = line.split(',').collect();
            let mut row = HashMap::new();
            for (i, h) in headers.iter().enumerate() {
                if let Some(&val) = cols.get(i) {
                    let v = if val == "\\N" {
                        None
                    } else {
                        Some(val.to_string())
                    };
                    row.insert(h.clone(), v);
                }
            }
            results.push(row);
        }
        Ok(results)
    }
}
