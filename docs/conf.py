import os
import sys

project = 'XKRT'
extensions = ['breathe']

# Path to doxygen xml output
breathe_projects = {
    "XKRT": "./build/xml"
}
breathe_default_project = "XKRT"

# https://sphinx-themes.org/
html_theme = 'furo'
