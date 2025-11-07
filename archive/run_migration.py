import os
from alembic.config import Config
from alembic import command

def run_migrations():
    # Path to the alembic.ini file
    alembic_ini_path = os.path.join(os.path.dirname(__file__), 'backend', 'alembic.ini')

    # Create Alembic config object
    alembic_cfg = Config(alembic_ini_path)

    # Run the upgrade command
    command.upgrade(alembic_cfg, "head")

if __name__ == "__main__":
    run_migrations()
