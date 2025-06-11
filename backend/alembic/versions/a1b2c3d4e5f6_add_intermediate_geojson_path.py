"""add intermediate_geojson_path to ingestion_jobs

Revision ID: a1b2c3d4e5f6
Revises: f3a4b5c6d7e8
Create Date: 2024-08-15 12:00:00.000000
"""
from alembic import op
import sqlalchemy as sa

# revision identifiers, used by Alembic.
revision = 'a1b2c3d4e5f6'
down_revision = 'f3a4b5c6d7e8'
branch_labels = None
depends_on = None

def upgrade():
    op.add_column('ingestion_jobs', sa.Column('intermediate_geojson_path', sa.String(), nullable=True))
    op.create_unique_constraint('uq_intermediate_geojson_path', 'ingestion_jobs', ['intermediate_geojson_path'])


def downgrade():
    op.drop_constraint('uq_intermediate_geojson_path', 'ingestion_jobs', type_='unique')
    op.drop_column('ingestion_jobs', 'intermediate_geojson_path')
