"""add_summary_fields_to_ingestion_jobs

Revision ID: f3a4b5c6d7e8
Revises: 2023_10_27
Create Date: 2024-08-15 10:00:00.000000

"""
from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = 'f3a4b5c6d7e8'
down_revision = '2023_10_27'
branch_labels = None
depends_on = None


def upgrade():
    op.add_column('ingestion_jobs', sa.Column('geocoding_summary', sa.JSON(), nullable=True))
    op.add_column('ingestion_jobs', sa.Column('complex_geometry_summary', sa.JSON(), nullable=True))
    op.add_column('ingestion_jobs', sa.Column('ner_summary', sa.JSON(), nullable=True))
    op.add_column('ingestion_jobs', sa.Column('content_security_summary', sa.JSON(), nullable=True))


def downgrade():
    op.drop_column('ingestion_jobs', 'content_security_summary')
    op.drop_column('ingestion_jobs', 'ner_summary')
    op.drop_column('ingestion_jobs', 'complex_geometry_summary')
    op.drop_column('ingestion_jobs', 'geocoding_summary')
