#!/usr/bin/env python3
"""
SPINDEX One-Click Launcher
Starts the entire SPINDEX stack with zero CLI interaction
"""

import subprocess
import sys
import time
import os
import webbrowser
from pathlib import Path

class Colors:
    """Terminal colors for pretty output"""
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

class SPINDEXLauncher:
    def __init__(self):
        self.base_dir = Path(__file__).parent
        self.backend_dir = self.base_dir / "backend"
        self.gui_dir = self.base_dir / "gui"
        self.processes = []

    def print_banner(self):
        """Print startup banner"""
        print(f"\n{Colors.HEADER}{Colors.BOLD}")
        print("╔══════════════════════════════════════════════════════════════╗")
        print("║                                                              ║")
        print("║                 🚀 SPINDEX LAUNCHER 🚀                       ║")
        print("║                                                              ║")
        print("║         One-Click Startup for Full-Stack System             ║")
        print("║                                                              ║")
        print("╚══════════════════════════════════════════════════════════════╝")
        print(f"{Colors.ENDC}\n")

    def print_step(self, step_num, total_steps, message):
        """Print a step in the startup process"""
        print(f"{Colors.OKCYAN}[{step_num}/{total_steps}]{Colors.ENDC} {message}")

    def print_success(self, message):
        """Print success message"""
        print(f"{Colors.OKGREEN}✅ {message}{Colors.ENDC}")

    def print_error(self, message):
        """Print error message"""
        print(f"{Colors.FAIL}❌ {message}{Colors.ENDC}")

    def print_info(self, message):
        """Print info message"""
        print(f"{Colors.OKBLUE}ℹ️  {message}{Colors.ENDC}")

    def check_prerequisites(self):
        """Check if required software is installed"""
        self.print_step(1, 6, "Checking prerequisites...")

        required = {
            'python3': 'Python 3.8+',
            'node': 'Node.js 16+',
            'npm': 'npm package manager'
        }

        missing = []
        for cmd, name in required.items():
            try:
                subprocess.run([cmd, '--version'],
                             capture_output=True,
                             check=True,
                             timeout=5)
                self.print_success(f"{name} found")
            except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired):
                self.print_error(f"{name} not found")
                missing.append(name)

        if missing:
            self.print_error(f"Missing required software: {', '.join(missing)}")
            self.print_info("Please install missing software and try again")
            return False

        self.print_success("All prerequisites satisfied")
        return True

    def install_backend_dependencies(self):
        """Install Python backend dependencies"""
        self.print_step(2, 6, "Installing backend dependencies...")

        requirements_file = self.backend_dir / "requirements.txt"
        if not requirements_file.exists():
            self.print_error(f"Requirements file not found: {requirements_file}")
            return False

        try:
            # Check if dependencies are already installed
            result = subprocess.run(
                [sys.executable, '-m', 'pip', 'check'],
                capture_output=True,
                text=True,
                cwd=self.backend_dir,
                timeout=10
            )

            if result.returncode == 0:
                self.print_success("Backend dependencies already installed")
                return True

            # Install dependencies
            self.print_info("Installing Python packages (this may take a minute)...")
            subprocess.run(
                [sys.executable, '-m', 'pip', 'install', '-r', str(requirements_file)],
                check=True,
                cwd=self.backend_dir,
                timeout=300
            )
            self.print_success("Backend dependencies installed")
            return True

        except subprocess.TimeoutExpired:
            self.print_error("Backend installation timed out")
            return False
        except subprocess.CalledProcessError as e:
            self.print_error(f"Failed to install backend dependencies: {e}")
            return False

    def install_frontend_dependencies(self):
        """Install Node.js frontend dependencies"""
        self.print_step(3, 6, "Installing frontend dependencies...")

        node_modules = self.gui_dir / "node_modules"
        package_json = self.gui_dir / "package.json"

        if not package_json.exists():
            self.print_error(f"package.json not found: {package_json}")
            return False

        if node_modules.exists():
            self.print_success("Frontend dependencies already installed")
            return True

        try:
            self.print_info("Installing npm packages (this may take a few minutes)...")
            subprocess.run(
                ['npm', 'install'],
                check=True,
                cwd=self.gui_dir,
                timeout=600
            )
            self.print_success("Frontend dependencies installed")
            return True

        except subprocess.TimeoutExpired:
            self.print_error("Frontend installation timed out")
            return False
        except subprocess.CalledProcessError as e:
            self.print_error(f"Failed to install frontend dependencies: {e}")
            return False

    def start_backend(self):
        """Start the FastAPI backend server"""
        self.print_step(4, 6, "Starting backend server...")

        try:
            # Start uvicorn in background
            process = subprocess.Popen(
                [sys.executable, '-m', 'uvicorn', 'main:app', '--reload', '--port', '8000'],
                cwd=self.backend_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )

            self.processes.append(('Backend', process))

            # Wait for backend to start
            self.print_info("Waiting for backend to start...")
            time.sleep(3)

            # Check if process is still running
            if process.poll() is not None:
                stderr = process.stderr.read()
                self.print_error(f"Backend failed to start: {stderr}")
                return False

            self.print_success("Backend server started on http://localhost:8000")
            self.print_info("API docs available at http://localhost:8000/docs")
            return True

        except Exception as e:
            self.print_error(f"Failed to start backend: {e}")
            return False

    def start_frontend(self):
        """Start the Vite development server"""
        self.print_step(5, 6, "Starting frontend server...")

        try:
            # Start Vite dev server in background
            process = subprocess.Popen(
                ['npm', 'run', 'dev'],
                cwd=self.gui_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )

            self.processes.append(('Frontend', process))

            # Wait for frontend to start
            self.print_info("Waiting for frontend to start...")
            time.sleep(5)

            # Check if process is still running
            if process.poll() is not None:
                stderr = process.stderr.read()
                self.print_error(f"Frontend failed to start: {stderr}")
                return False

            self.print_success("Frontend server started on http://localhost:8080")
            return True

        except Exception as e:
            self.print_error(f"Failed to start frontend: {e}")
            return False

    def open_browser(self):
        """Open the web UI in default browser"""
        self.print_step(6, 6, "Opening web interface...")

        try:
            time.sleep(2)  # Give servers a moment to fully start
            webbrowser.open('http://localhost:8080')
            self.print_success("Browser opened to SPINDEX web interface")
            return True
        except Exception as e:
            self.print_error(f"Failed to open browser: {e}")
            self.print_info("Please manually open: http://localhost:8080")
            return False

    def print_status(self):
        """Print final status and instructions"""
        print(f"\n{Colors.OKGREEN}{Colors.BOLD}")
        print("╔══════════════════════════════════════════════════════════════╗")
        print("║                                                              ║")
        print("║              ✅ SPINDEX IS NOW RUNNING ✅                    ║")
        print("║                                                              ║")
        print("╚══════════════════════════════════════════════════════════════╝")
        print(f"{Colors.ENDC}")

        print(f"\n{Colors.BOLD}Access URLs:{Colors.ENDC}")
        print(f"  🌐 Web UI:       {Colors.OKCYAN}http://localhost:8080{Colors.ENDC}")
        print(f"  🔌 Backend API:  {Colors.OKCYAN}http://localhost:8000{Colors.ENDC}")
        print(f"  📚 API Docs:     {Colors.OKCYAN}http://localhost:8000/docs{Colors.ENDC}")

        print(f"\n{Colors.BOLD}Features Available:{Colors.ENDC}")
        print(f"  🔍 Advanced Search with AVX-512 acceleration")
        print(f"  📤 Drag-and-drop file upload")
        print(f"  📊 Live dashboard with real-time stats")
        print(f"  📑 Index management")
        print(f"  📜 Query history")

        print(f"\n{Colors.BOLD}To Stop SPINDEX:{Colors.ENDC}")
        print(f"  Press {Colors.WARNING}Ctrl+C{Colors.ENDC} in this terminal")

        print(f"\n{Colors.OKBLUE}Logs are being written to this terminal...{Colors.ENDC}\n")

    def cleanup(self):
        """Cleanup and stop all processes"""
        print(f"\n{Colors.WARNING}Shutting down SPINDEX...{Colors.ENDC}")

        for name, process in self.processes:
            if process.poll() is None:  # Process is still running
                self.print_info(f"Stopping {name}...")
                process.terminate()
                try:
                    process.wait(timeout=5)
                    self.print_success(f"{name} stopped")
                except subprocess.TimeoutExpired:
                    self.print_warning(f"{name} didn't stop gracefully, forcing...")
                    process.kill()

        self.print_success("All services stopped")
        print(f"\n{Colors.OKGREEN}Thank you for using SPINDEX!{Colors.ENDC}\n")

    def run(self):
        """Main execution flow"""
        try:
            self.print_banner()

            # Check prerequisites
            if not self.check_prerequisites():
                return 1

            # Install dependencies (one-time setup)
            if not self.install_backend_dependencies():
                return 1

            if not self.install_frontend_dependencies():
                return 1

            # Start services
            if not self.start_backend():
                return 1

            if not self.start_frontend():
                return 1

            # Open browser
            self.open_browser()

            # Print status
            self.print_status()

            # Keep running until Ctrl+C
            try:
                while True:
                    # Check if processes are still running
                    for name, process in self.processes:
                        if process.poll() is not None:
                            self.print_error(f"{name} stopped unexpectedly!")
                            return 1
                    time.sleep(5)

            except KeyboardInterrupt:
                print()  # New line after ^C

            return 0

        except Exception as e:
            self.print_error(f"Unexpected error: {e}")
            return 1

        finally:
            self.cleanup()

def main():
    """Entry point"""
    launcher = SPINDEXLauncher()
    sys.exit(launcher.run())

if __name__ == '__main__':
    main()
