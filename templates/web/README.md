# {{project_name}}

A responsive dashboard example using the YAZA Current visual foundation.

## Project layout

- `src/main.yz` contains the YAZA command-line sample.
- `web/index.html` contains the dashboard, chart studio, component showcase, and browser-native demo interactions.
- `web/yaza-ui.css` contains the YAZA-authored design tokens, responsive components, and chart animations.
- `web/yaza-ui-icon-atlas-v2.png` is the generated YAZA glass icon artwork used by the showcase.
- `yaza.toml` stores project metadata.

## What the example contains

The dashboard shows a workspace sidebar, responsive navigation, top bar, search, theme switch, metric cards, visitor trends, traffic-source breakdown, project table, status filters, pagination, a create-project dialog, and a CSV export. The searchable chart gallery contains 27 visual patterns: line, area, stacked area, vertical and horizontal bars, grouped columns, stacked bars, combo, histogram, box-and-whisker, donut, pie, radial progress, gauge, scatter, bubble, radar, heatmap, candlestick, funnel, treemap, waterfall, timeline/Gantt, Sankey, Pareto, sunburst, and bullet/range. Presets and custom height and minimum-card-width sliders resize the previews and change the responsive grid density.

The component studio includes action buttons, icon buttons, badges, alerts, inputs, selects, textarea validation and character count, checkbox, radio group, switch, avatars, anchor navigation, disclosure, progress, skeleton loading, empty state, dialog, toast, and an original YAZA-colored icon illustration. Button and input previews offer compact, regular, and large sizes.

Several preview interactions work in the browser: switch between light and dark themes, choose the chart range and chart size, search chart patterns, search and filter sample projects, move through result pages, resize button and input examples, create an in-memory sample project, dismiss feedback, and download the sample project list as CSV. Chart strokes, bars, dots, cards, and controls use restrained spring-like motion; reduced-motion preferences are respected. Data is demonstration content held in the page. Reloading restores the original examples; the page is not connected to an API, database, or YAZA application state.

The page and stylesheet have no framework or remote asset dependency. They use semantic HTML, visible keyboard focus, responsive layouts, reduced-motion support, and a skip link. Data charts are inline SVG and CSS; the icon illustration is a local PNG. Native browser behavior is used for dialog controls, form inputs, checkbox, radio, select, and disclosure.

## YAZA UI status

This is a comprehensive visual and interaction example for the current web starter, not a production component library. It does not yet provide a YAZA UI DSL, compiler-integrated browser renderer, reusable package API, automated accessibility audit, visual regression pipeline, or Android/iOS renderer. The mobile UI proposal describes future work; this template does not imply that a mobile build target is available.

## Run the YAZA sample

```sh
yaza run src/main.yz
```

The current compiler does not compile YAZA to JavaScript or WebAssembly. HTML, CSS, and page-local JavaScript are static web assets, independent from the YAZA program.
