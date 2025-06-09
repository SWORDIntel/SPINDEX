from logging.config import fileConfig

from sqlalchemy import engine_from_config
from sqlalchemy import pool

from alembic import context

# this is the Alembic Config object, which provides
# access to the values within the .ini file in use.
config = context.config

# Interpret the config file for Python logging.
# This line needs to be setup properly if other config files are used.
# fileConfig(config.config_file_name) # Commented out if not using complex logging setup via ini for alembic

# add your model's MetaData object here
# for 'autogenerate' support
# from myapp import mymodel
# target_metadata = mymodel.Base.metadata
# For SPINDEX, we need to import Base from our models.py
# Assuming this env.py is in alembic/ and models.py is in backend/
# we need to adjust sys.path or use relative imports carefully if alembic/ is a module.
# A common pattern is to add the project root to sys.path.
import os
import sys
# Add the parent directory of 'alembic' (which is 'backend') to sys.path
# This allows `from backend.models import Base` to work
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from backend.models import Base # Import your Base
from backend.database import SQLALCHEMY_DATABASE_URL # Import your database URL

target_metadata = Base.metadata

# other values from the config, defined by the needs of env.py,
# can be acquired:
# my_important_option = config.get_main_option("my_important_option")
# ... etc.


def run_migrations_offline():
    """Run migrations in 'offline' mode.

    This configures the context with just a URL
    and not an Engine, though an Engine is acceptable
    here as well.  By skipping the Engine creation
    we don't even need a DBAPI to be available.

    Calls to context.execute() here emit the given string to the
    script output.

    """
    # Use the database URL from database.py if sqlalchemy.url is not set in alembic.ini
    # However, alembic.ini should be the primary source.
    # If you want to ensure it uses the one from database.py, you can set it here:
    # url = SQLALCHEMY_DATABASE_URL
    # But typically, alembic.ini's sqlalchemy.url is used.
    # For this setup, we assume alembic.ini has the correct sqlalchemy.url
    url = config.get_main_option("sqlalchemy.url")
    if not url: # Fallback if not in alembic.ini, though it should be
        url = SQLALCHEMY_DATABASE_URL

    context.configure(
        url=url,
        target_metadata=target_metadata,
        literal_binds=True,
        dialect_opts={"paramstyle": "named"},
    )

    with context.begin_transaction():
        context.run_migrations()


def run_migrations_online():
    """Run migrations in 'online' mode.

    In this scenario we need to create an Engine
    and associate a connection with the context.

    """
    # Ensure the sqlalchemy.url from alembic.ini is used
    # connectable = engine_from_config(
    #     config.get_section(config.config_ini_section), # This gets the [alembic] section by default
    #     prefix="sqlalchemy.", # Alembic keys in ini are prefixed with sqlalchemy.
    #     poolclass=pool.NullPool,
    # )

    # A more direct way if you are sure about the section name (usually 'alembic')
    # and your sqlalchemy.url is correctly set in alembic.ini
    ini_section = config.get_section(config.config_ini_section)
    db_url = ini_section.get('sqlalchemy.url')
    if not db_url: # Fallback, though ini should be primary
        db_url = SQLALCHEMY_DATABASE_URL

    connectable = engine_from_config(
        {"sqlalchemy.url": db_url}, # Pass the URL directly
        prefix="sqlalchemy.",
        poolclass=pool.NullPool
    )


    with connectable.connect() as connection:
        context.configure(
            connection=connection, target_metadata=target_metadata
        )

        with context.begin_transaction():
            context.run_migrations()


if context.is_offline_mode():
    run_migrations_offline()
else:
    run_migrations_online()
