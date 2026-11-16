# Contributing

Ninja Turtle is **untested on hardware**. Reports from anyone who wires it to a real printer, AFC-Pro or Box Turtle are
the most useful contribution — including "it didn't connect".

## Reporting

- **Hardware test report / bug:** open an issue using the template. Include the printer model and firmware version,
  the Ninja Turtle commit, what you wired (see `docs/afc-pro-port.md`), what the printer screen showed, and any
  `vmmu`/bridge logs.
- **Not here:** problems with a genuine Prusa MMU3 or stock printer firmware belong with Prusa, not this project.
  Do not report Ninja Turtle issues to Prusa or Armored Turtle.

## Development

Everything builds in the dev container that CI uses:

```bash
scripts/dev ./scripts/ci
```

That builds the host tools and tests, runs the full test suite (upstream unit tests, Ninja Turtle unit tests,
closed-loop `vprinter` ⇄ `vmmu` tests), and cross-builds the AFC-Pro firmware. A pull request should keep it green.
See [`docs/development.md`](docs/development.md) for the layout and simulators.

Guidelines:

- Keep the upstream MMU3 logic and protocol code (`src/logic`, `src/modules/protocol*`) unchanged where possible;
  hardware differences belong in the HAL, `src/nt/`, or `ports/`. Guard Ninja Turtle changes to upstream files with
  `NT_STM32` / `NT_BOXTURTLE` so the original AVR build is unaffected.
- The firmware must keep reporting version 3.0.4 — the printer refuses any other MMU version.
- New behaviour gets a test: a unit test in `tests/unit/nt`, or a closed-loop scenario in `tools/vprinter`.
- Code style: `.clang-format` (C/C++), `.cmake-format.py` (CMake).

## License

Contributions are accepted under GPL-3.0, the license of the upstream Prusa-Firmware-MMU this project forks.
