---
name: git-pr-creator
description: Create GitHub Pull Requests using `gh pr create`. This skill analyzes the diff between the current branch and the base branch (default: `main`) to generate a descriptive PR title and body. Use when the user is ready to submit their changes for review.
---

# Git PR Creator

This skill automates the creation of GitHub Pull Requests using the `gh` (GitHub CLI) tool. It crafts a concise title and a detailed body by analyzing commit messages and file differences.

## Workflow

1.  **Analyze changes**:
    *   Find the base branch (default is `main`, but can be specified).
    *   Use the helper script `.agents/skills/git-pr-creator/scripts/summarize_changes.sh [base_branch]` to get a summary of commits and diffs.
2.  **Generate PR Title**:
    *   Format: `[action]: [description]`
    *   Examples: `feat: add user authentication`, `fix: correct layout on mobile`, `refactor: clean up database queries`.
3.  **Generate PR Body**:
    *   Use a simple Markdown template.
    *   **Summary**: A one-sentence high-level overview.
    *   **Changes**: A bulleted list of key technical changes.
    *   **Testing**: Briefly describe how the changes were verified.
4.  **Execute command**:
    *   `gh pr create --title "[action]: [description]" --body "[PR_BODY_CONTENT]"`
    *   Optionally add `--draft` if requested by the user.

## Helper Scripts

*   [summarize_changes.sh](file:///.agents/skills/git-pr-creator/scripts/summarize_changes.sh): Summarizes commits and diffs for easier analysis.

## Examples

**Example PR Generation:**
- Current Branch: `feature/new-login`
- Base Branch: `main`
- Commits found: `feat: implement login service`, `feat: add login UI`
- Generated Command:
  ```bash
  gh pr create --title "feat: implement user login functionality" --body "## Summary
  Implements a complete login flow using JWT.

  ## Changes
  - Added AuthService for JWT management.
  - Created LoginView with responsive UI.
  - Integrated login endpoint with backend.

  ## Testing
  - Manually tested login with valid/invalid credentials."
  ```

## Critical Rule
- Ensure you have the `gh` tool installed and authorized before execution.
- If multiple base branches are potential candidates, ask the user or default to `main`.
- Copy or link relevant information from the `git-commit-creator` skill to keep titles consistent with commits.
