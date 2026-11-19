/* YAZA Live — Zero-Dependency Progressive Hypermedia Fragment Swapper (Strict CSP Safe) */
(() => {
  "use strict";

  const root = document.documentElement;
  const savedTheme = localStorage.getItem("yaza-theme");
  if (savedTheme === "dark" || savedTheme === "light") {
    root.setAttribute("data-theme", savedTheme);
  }

  document.addEventListener("click", (ev) => {
    const toggle = ev.target.closest("[data-yaza-theme-toggle]");
    if (toggle) {
      const next = root.getAttribute("data-theme") === "dark" ? "light" : "dark";
      root.setAttribute("data-theme", next);
      localStorage.setItem("yaza-theme", next);
      return;
    }

    const link = ev.target.closest("a[data-yaza-target]");
    if (!link || ev.metaKey || ev.ctrlKey || ev.shiftKey) return;
    const targetSel = link.getAttribute("data-yaza-target");
    const targetEl = targetSel ? document.querySelector(targetSel) : null;
    if (!targetEl) return;

    ev.preventDefault();
    swapFragment(link.getAttribute("href") || "/", { method: "GET" }, targetSel, targetEl);
  });

  document.addEventListener("submit", (ev) => {
    const form = ev.target.closest("form[data-yaza-target]");
    if (!form) return;
    const targetSel = form.getAttribute("data-yaza-target");
    const targetEl = targetSel ? document.querySelector(targetSel) : null;
    if (!targetEl) return;

    ev.preventDefault();
    const action = form.getAttribute("action") || window.location.pathname;
    const method = (form.getAttribute("method") || "POST").toUpperCase();
    const params = new URLSearchParams(new FormData(form));
    swapFragment(
      action,
      {
        method,
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body: method === "GET" ? undefined : params.toString(),
      },
      targetSel,
      targetEl
    );
  });

  async function swapFragment(url, init, targetSel, targetEl) {
    targetEl.classList.add("yz-is-loading");
    try {
      const headers = Object.assign(
        {
          Accept: "text/vnd.yaza.fragment, text/html",
          "X-Yaza-Fragment": targetSel,
        },
        init.headers || {}
      );
      const res = await fetch(url, Object.assign({}, init, { headers, credentials: "same-origin" }));
      if (res.redirected) {
        window.location.href = res.url;
        return;
      }
      if (!res.ok) {
        window.location.reload();
        return;
      }
      const html = await res.text();
      const tpl = document.createElement("template");
      tpl.innerHTML = html.trim();
      const replacement = tpl.content.firstElementChild;
      if (replacement) {
        targetEl.replaceWith(replacement);
      }
    } catch (_err) {
      window.location.reload();
    }
  }
})();
