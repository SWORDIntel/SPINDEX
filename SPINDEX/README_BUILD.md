. Build Documentation (README_BUILD.md)

This file provides explicit instructions for developers, CI/CD systems, or anyone else needing to build and test the project.

README_BUILD.md
Markdown

# Project Build and Test Instructions

This document outlines the necessary steps to set up the environment, build all components, and run the tests for this project.

## 1. Project Structure

The build and test process assumes the following directory structure:

.
├── build_and_test.sh           # This master script
├── README_BUILD.md             # This documentation
├── requirements.txt            # Python dependencies
├── stego_test.py               # Main Python application logic (assumed)
├── test_jsteg.py               # Python unit tests
├── test_images_jpeg/
│   ├── jpeg_grayscale.jpg      # Test image
│   └── jpeg_low_quality_small.jpg # Test image
├── Makefile                    # Optional: For building C components
└── src/                        # Optional: Directory for C source files


## 2. Prerequisites

Before running the build script, ensure you have the following software installed on your system:

* **Python 3**: The script is written to use `python3`.
* **pip 3**: The Python package installer.
* **git**: For version control diagnostics.
* **gcc** (or a compatible C compiler): Required if the project includes C components managed by a `Makefile`.

## 3. How to Build and Test

The entire process is automated by the `build_and_test.sh` script.

### One-Step Execution

To set up the environment, install all dependencies, build any C components, and run all tests, simply execute the script from the root of the repository:

```sh
# Make the script executable (only need to do this once)
chmod +x build_and_test.sh

# Run the script
./build_and_test.sh

What the Script Does

The build_and_test.sh script performs the following actions:

    Initial Diagnostics: It checks for required tools (python3, gcc), verifies the repository state with git, and confirms that necessary files and directories (test_jsteg.py, test_images_jpeg/) are present.
    C Component Build: If a Makefile is found in the root directory, it runs the make command to compile any C programs. This standardizes the C build process.
    Python Environment Setup:
        It creates a Python virtual environment named keyplug_venv if one doesn't already exist. Using a virtual environment prevents conflicts with system-wide packages.
        It activates the environment.
        It installs the exact versions of the Python libraries specified in requirements.txt using pip.
    Run Python Tests: It executes the test_jsteg.py script using the Python interpreter from the virtual environment, ensuring it uses the correct dependencies.
    Cleanup: The script deactivates the virtual environment upon successful completion.

This automated process ensures a reliable and repeatable build, which is essential for development, collaboration, and continuous integration (CI/CD).
