#!/usr/bin/env bash
set -euo pipefail

# Creates the gtav conda environment for running convert.py.

source /opt/miniconda/etc/profile.d/conda.sh

conda create -n gtav python=3.11 -y
conda activate gtav

pip install numpy pillow
