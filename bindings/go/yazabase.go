// Package yazabase provides a Go client for YazaBase embedded database.
package yazabase

import (
	"bytes"
	"encoding/csv"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
)

type DB struct {
	dirPath string
	dbName  string
	cliPath string
}

func Open(dirPath, dbName string) (*DB, error) {
	absDir, err := filepath.Abs(dirPath)
	if err != nil {
		return nil, err
	}
	if err := os.MkdirAll(absDir, 0755); err != nil {
		return nil, err
	}

	cliPath := "yazabase"
	candidates := []string{
		"yazabase",
		"yazabase-cli",
		filepath.Join(os.Getenv("HOME"), ".yaza/bin/yazabase"),
		"/usr/local/bin/yazabase",
	}
	for _, c := range candidates {
		if path, err := exec.LookPath(c); err == nil {
			cliPath = path
			break
		}
	}

	db := &DB{dirPath: absDir, dbName: dbName, cliPath: cliPath}
	dbFile := filepath.Join(absDir, dbName+".ydb")
	if _, err := os.Stat(dbFile); os.IsNotExist(err) {
		cmd := exec.Command(cliPath, "init", absDir, dbName)
		if out, err := cmd.CombinedOutput(); err != nil {
			return nil, fmt.Errorf("yazabase init failed: %s: %w", string(out), err)
		}
	}
	return db, nil
}

func (db *DB) Query(sql string) ([]map[string]interface{}, error) {
	cmd := exec.Command(db.cliPath, "sql", "--csv", db.dirPath, db.dbName, sql)
	var out bytes.Buffer
	var stderr bytes.Buffer
	cmd.Stdout = &out
	cmd.Stderr = &stderr

	if err := cmd.Run(); err != nil {
		return nil, fmt.Errorf("yazabase query error: %s: %w", stderr.String(), err)
	}

	r := csv.NewReader(strings.NewReader(out.String()))
	records, err := r.ReadAll()
	if err != nil {
		return nil, err
	}
	if len(records) == 0 {
		return nil, nil
	}

	headers := records[0]
	var results []map[string]interface{}
	for _, row := range records[1:] {
		m := make(map[string]interface{})
		for i, h := range headers {
			if row[i] == "\\N" {
				m[h] = nil
			} else {
				m[h] = row[i]
			}
		}
		results = append(results, m)
	}
	return results, nil
}
