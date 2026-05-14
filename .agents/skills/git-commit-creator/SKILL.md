---
name: git-commit-creator
description: Generate standardized git commit messages in the format `[action]: [description]`. Trigger this skill when the user wants to commit changes, needs a commit message, or is preparing a git commit. Ensure the message follows the `[action]: [what]` format using predefined action types like `init`, `feat`, or `fix`.
---

# Git Commit Creator

This skill helps you generate consistent and clear git commit messages following a specific convention: `[action]: [description]`.

## Methodology

When generating a commit message:
1. **Analyze the changes**: Use `git diff --cached` (or similar tools) to understand what changes are being committed.
2. **Determine the action**: Choose the most appropriate action type from the list below.
3. **Write the description**: Summarize the change succinctly in lowercase, starting with an imperative verb (e.g., "add", "fix", "update").

## Action Types

Use the following action types based on the nature of the change:

- **init**: For the project initialization or first commit.
- **feat**: A new feature or significant addition.
- **fix**: A bug fix.
- **docs**: Documentation only changes.
- **style**: Changes that do not affect the meaning of the code (white-space, formatting, missing semi-colons, etc).
- **refactor**: A code change that neither fixes a bug nor adds a feature.
- **test**: Adding missing tests or correcting existing tests.
- **chore**: Changes to the build process or auxiliary tools and libraries such as documentation generation.

## Format

The output must be a single line following this exact template:
`[action]: [description]`

## Examples

**Example 1: Initial commit**
Input: Project setup with basic structure
Output: `init: initialize project structure`

**Example 2: Adding a new login feature**
Input: Implemented JWT authentication for user login
Output: `feat: add jwt authentication for user login`

**Example 3: Fixing a crash**
Input: Resolved null pointer exception in payment processing
Output: `fix: resolved null pointer exception in payment processing`

**Example 4: Updating README**
Input: Added installation instructions to README.md
Output: `docs: update installation instructions in readme`
