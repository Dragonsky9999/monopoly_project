@echo off
cd /d "%~dp0.."
set OUT=%~dp0git_push_console.txt
echo == status == > "%OUT%"
git status --short >> "%OUT%" 2>&1
git add -A >> "%OUT%" 2>&1
git commit -m "GUI: table view, rules update, demo log" -m "- Qt table UI (board-centered, drag & drop money/cards, hover panels, text size)" -m "- Rules: doubles up to 3, utility re-roll, monopoly double rent, debt must be resolved, game ends on first bankruptcy (continue possible)" -m "- Negotiation ends with one button, player order drag in new-game dialog, 1-based player numbers" -m "- Auto record to Documents/MonopolyRecorder/monopoly_<datetime>.log" -m "- Demo game log (tools/demo_full_game.log) and generator" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -m "Claude-Session: https://claude.ai/code/session_01K1ntBWo9kxdNEpSxoP1wLM" >> "%OUT%" 2>&1
echo == push == >> "%OUT%"
git push origin main >> "%OUT%" 2>&1
echo EXIT=%ERRORLEVEL% >> "%OUT%"
git log --oneline -3 >> "%OUT%" 2>&1
