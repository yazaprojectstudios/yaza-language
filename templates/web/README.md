# {{project_name}} — YAZA Web Starter

Lightweight single-binary **Arena-SSR Web Server** using `std/web/*`, **YAZA UI** (`public/yaza-ui.css`), and strict Web CSP.

## Quick Start
```sh
yaza check
yaza build --release src/main.yz -o {{project_name}}
YAZA_SELF_TEST=1 ./{{project_name}}
./{{project_name}}
# Open http://127.0.0.1:8080/
```

Need embedded YazaBase SQL, Argon2id Login, CSRF, and full MVC layers? Scaffold with `yaza new --mvc my_app`.
