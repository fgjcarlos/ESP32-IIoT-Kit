
# Repo Rename Specification

## Purpose

The GitHub repository MUST be renamed from `ESP32-fg` to `ESP32-IIoT-Kit`. All internal documentation references to the old repo name MUST be updated to the new name.

## Requirements

### Requirement: Repository Name Update

`mkdocs.yml`, `README.md`, `docs_site/index.md`, and any file containing `ESP32-fg` or `ESP32-fg` as a URL segment MUST be updated to reference `ESP32-IIoT-Kit`. The GitHub Pages URL changes from `fgjcarlos.github.io/ESP32-fg` to `fgjcarlos.github.io/ESP32-IIoT-Kit`.

#### Scenario: MkDocs site builds with correct URLs

- GIVEN `mkdocs.yml` is updated with the new repo name and site URL
- WHEN `mkdocs build` is run
- THEN the build succeeds with zero errors
- AND generated HTML contains no references to the old `ESP32-fg` URL

#### Scenario: ADR documents the transformation

- GIVEN `knowledge/Decisiones/004-iiot-kit-transformation.md` exists
- WHEN a developer reads it
- THEN they find the rationale for the rename, the trade-offs considered, and the date of the decision

### Requirement: MkDocs Site Name and Navigation

`mkdocs.yml` MUST set `site_name: ESP32-IIoT-Kit`, update `site_url` to the new GitHub Pages URL, and reflect the updated nav structure (Fase 4 renamed to "Dashboard Embebido + API REST").

#### Scenario: Landing page shows new project identity

- GIVEN a visitor opens the GitHub Pages site
- WHEN the index page loads
- THEN the site title reads "ESP32-IIoT-Kit" and the description describes a generic IIoT platform

---

