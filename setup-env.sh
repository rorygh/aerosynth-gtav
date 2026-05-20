#!/usr/bin/env bash
set -euo pipefail

# Creates the gtav conda environment for running convert.py.

source /opt/miniconda/etc/profile.d/conda.sh

conda create -n gtav python=3.11 -y
conda activate gtav

pip install numpy pillow

# -- Optional: download pre-captured output instead of capturing from scratch --
# FILE_ID="1qaFDvfg5wpU7Gg2x_FdTl4Cs_e-5RDtD"
# curl -L "https://drive.google.com/uc?export=download&id=${FILE_ID}" -o /tmp/gtav_output.zip
# unzip /tmp/gtav_output.zip -d /workspace/aerosynth/aerosynth-gtav/output/
# rm /tmp/gtav_output.zip
