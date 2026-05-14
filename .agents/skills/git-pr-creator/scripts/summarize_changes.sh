#!/bin/bash
# Summarize changes between current branch and main
# Usage: ./summarize_changes.sh [base_branch]

BASE_BRANCH=${1:-main}
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

echo "### Current Branch: $CURRENT_BRANCH"
echo "### Base Branch: $BASE_BRANCH"
echo ""
echo "#### Recent Commits (Since $BASE_BRANCH):"
git log $BASE_BRANCH..$CURRENT_BRANCH --oneline

echo ""
echo "#### Changed Files:"
git diff $BASE_BRANCH..$CURRENT_BRANCH --stat

echo ""
echo "#### Diff Summary (First 50 lines per file):"
git diff $BASE_BRANCH..$CURRENT_BRANCH
