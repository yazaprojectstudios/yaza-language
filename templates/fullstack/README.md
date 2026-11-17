# {{project_name}}

A starter workspace layout for a YAZA API, web client, and shared package.

## Project layout

- `apps/api/src/main.yz` is the API command-line sample.
- `apps/web/src/main.yz` is the web command-line sample.
- `apps/web/public/index.html` is a static page shell.
- `packages/shared/` is reserved for code shared by the API and web client.

## Run the samples

```sh
yaza run apps/api/src/main.yz
yaza run apps/web/src/main.yz
```

Workspace builds, cross-project imports, API hosting, and web compilation are not implemented yet. Each YAZA entry point can currently be built on its own, and the HTML remains static.
