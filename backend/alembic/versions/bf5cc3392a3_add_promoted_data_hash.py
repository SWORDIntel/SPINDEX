"""Add promoted_data_hash to IngestionJobDB

Revision ID: bf5cc3392a3
Revises: a1b2c3d4e5f6
Create Date: 2025-07-22 14:08:34.951055
"""
from alembic import op
import sqlalchemy as sa

# revision identifiers, used by Alembic.
revision = 'bf5cc3392a3'
down_revision = 'a1b2c3d4e5f6'
branch_labels = None
depends_on = None

def upgrade():
    op.add_column('ingestion_jobs', sa.Column('promoted_data_hash', sa.String(), nullable=True))


def downgrade():
    op.drop_column('ingestion_jobs', 'promoted_data_hash')
