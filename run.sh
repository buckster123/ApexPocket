#!/bin/bash
# Run Claudeagotchi

cd "$(dirname "$0")"

# Check for virtual environment
if [ -d "venv" ]; then
    source venv/bin/activate
fi

# Run
python src/main.py "$@"
