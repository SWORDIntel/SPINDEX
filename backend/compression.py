"""
SPINDEX Compression Support Module
Automatic decompression of uploaded compressed files
"""

import gzip
import zipfile
import tarfile
import logging
import os
from pathlib import Path
from typing import Optional, List, Tuple
import shutil

logger = logging.getLogger(__name__)


class CompressionHandler:
    """Handle compressed file formats"""

    SUPPORTED_FORMATS = {
        '.gz': 'gzip',
        '.zip': 'zip',
        '.tar': 'tar',
        '.tar.gz': 'tar.gz',
        '.tgz': 'tar.gz',
        '.tar.bz2': 'tar.bz2',
        '.tbz2': 'tar.bz2',
        '.tar.xz': 'tar.xz',
        '.txz': 'tar.xz'
    }

    @staticmethod
    def is_compressed(filename: str) -> bool:
        """Check if file is a compressed format"""
        filename_lower = filename.lower()

        # Check for compound extensions first (.tar.gz, etc.)
        for ext in ['.tar.gz', '.tar.bz2', '.tar.xz']:
            if filename_lower.endswith(ext):
                return True

        # Check single extensions
        ext = Path(filename).suffix.lower()
        return ext in CompressionHandler.SUPPORTED_FORMATS

    @staticmethod
    def get_compression_type(filename: str) -> Optional[str]:
        """
        Get compression type from filename
        Returns: 'gzip', 'zip', 'tar', 'tar.gz', 'tar.bz2', 'tar.xz', or None
        """
        filename_lower = filename.lower()

        # Check compound extensions first
        for ext, comp_type in CompressionHandler.SUPPORTED_FORMATS.items():
            if filename_lower.endswith(ext):
                return comp_type

        return None

    @staticmethod
    def decompress_gzip(input_path: str, output_dir: str) -> List[str]:
        """
        Decompress a gzip file
        Returns list of extracted file paths
        """
        extracted_files = []

        try:
            output_filename = Path(input_path).stem  # Remove .gz
            output_path = os.path.join(output_dir, output_filename)

            with gzip.open(input_path, 'rb') as f_in:
                with open(output_path, 'wb') as f_out:
                    shutil.copyfileobj(f_in, f_out)

            extracted_files.append(output_path)
            logger.info(f"Decompressed gzip: {input_path} -> {output_path}")

        except Exception as e:
            logger.error(f"Failed to decompress gzip {input_path}: {e}")
            raise

        return extracted_files

    @staticmethod
    def decompress_zip(input_path: str, output_dir: str, max_files: int = 100) -> List[str]:
        """
        Decompress a zip file
        Returns list of extracted file paths

        Security: Limits number of extracted files to prevent zip bombs
        """
        extracted_files = []

        try:
            with zipfile.ZipFile(input_path, 'r') as zip_ref:
                # Security check: limit number of files
                file_list = zip_ref.namelist()
                if len(file_list) > max_files:
                    raise ValueError(f"Zip contains too many files ({len(file_list)} > {max_files})")

                # Security check: validate file sizes
                total_size = sum(info.file_size for info in zip_ref.infolist())
                max_total_size = 500 * 1024 * 1024  # 500MB total
                if total_size > max_total_size:
                    raise ValueError(f"Zip content too large ({total_size} bytes)")

                # Extract files
                for member in file_list:
                    # Security: prevent directory traversal
                    member_path = os.path.normpath(os.path.join(output_dir, member))
                    if not member_path.startswith(os.path.abspath(output_dir)):
                        logger.warning(f"Skipping potentially malicious path: {member}")
                        continue

                    # Extract file
                    zip_ref.extract(member, output_dir)
                    extracted_path = os.path.join(output_dir, member)

                    # Only track actual files, not directories
                    if os.path.isfile(extracted_path):
                        extracted_files.append(extracted_path)

                logger.info(f"Decompressed zip: {input_path} -> {len(extracted_files)} files")

        except Exception as e:
            logger.error(f"Failed to decompress zip {input_path}: {e}")
            raise

        return extracted_files

    @staticmethod
    def decompress_tar(input_path: str, output_dir: str, max_files: int = 100) -> List[str]:
        """
        Decompress a tar file (including .tar.gz, .tar.bz2, .tar.xz)
        Returns list of extracted file paths

        Security: Limits number of extracted files and validates paths
        """
        extracted_files = []

        try:
            with tarfile.open(input_path, 'r:*') as tar_ref:
                # Security check: limit number of files
                members = tar_ref.getmembers()
                if len(members) > max_files:
                    raise ValueError(f"Tar contains too many files ({len(members)} > {max_files})")

                # Security check: validate file sizes
                total_size = sum(member.size for member in members)
                max_total_size = 500 * 1024 * 1024  # 500MB total
                if total_size > max_total_size:
                    raise ValueError(f"Tar content too large ({total_size} bytes)")

                # Extract files
                for member in members:
                    # Security: prevent directory traversal
                    member_path = os.path.normpath(os.path.join(output_dir, member.name))
                    if not member_path.startswith(os.path.abspath(output_dir)):
                        logger.warning(f"Skipping potentially malicious path: {member.name}")
                        continue

                    # Extract member
                    tar_ref.extract(member, output_dir, filter='data')
                    extracted_path = os.path.join(output_dir, member.name)

                    # Only track actual files, not directories
                    if os.path.isfile(extracted_path):
                        extracted_files.append(extracted_path)

                logger.info(f"Decompressed tar: {input_path} -> {len(extracted_files)} files")

        except Exception as e:
            logger.error(f"Failed to decompress tar {input_path}: {e}")
            raise

        return extracted_files

    @staticmethod
    def decompress(input_path: str, output_dir: str) -> Tuple[bool, List[str], Optional[str]]:
        """
        Automatically decompress a file based on its extension

        Args:
            input_path: Path to compressed file
            output_dir: Directory to extract files to

        Returns:
            (success: bool, extracted_files: List[str], error_message: Optional[str])
        """
        if not os.path.exists(input_path):
            return False, [], f"File not found: {input_path}"

        if not os.path.exists(output_dir):
            os.makedirs(output_dir, exist_ok=True)

        filename = os.path.basename(input_path)
        compression_type = CompressionHandler.get_compression_type(filename)

        if not compression_type:
            return False, [], f"Unsupported or uncompressed file: {filename}"

        try:
            extracted_files = []

            if compression_type == 'gzip':
                extracted_files = CompressionHandler.decompress_gzip(input_path, output_dir)

            elif compression_type == 'zip':
                extracted_files = CompressionHandler.decompress_zip(input_path, output_dir)

            elif compression_type in ['tar', 'tar.gz', 'tar.bz2', 'tar.xz']:
                extracted_files = CompressionHandler.decompress_tar(input_path, output_dir)

            else:
                return False, [], f"Unsupported compression type: {compression_type}"

            return True, extracted_files, None

        except ValueError as e:
            # Security-related errors (zip bomb, too many files, etc.)
            error_msg = f"Security validation failed: {e}"
            logger.error(error_msg)
            return False, [], error_msg

        except Exception as e:
            error_msg = f"Decompression failed: {e}"
            logger.error(error_msg)
            return False, [], error_msg

    @staticmethod
    def get_archive_info(input_path: str) -> Optional[dict]:
        """
        Get information about a compressed archive without extracting it

        Returns:
            {
                'type': 'gzip' | 'zip' | 'tar',
                'file_count': int,
                'total_size': int,
                'files': List[str]
            }
        """
        if not os.path.exists(input_path):
            return None

        filename = os.path.basename(input_path)
        compression_type = CompressionHandler.get_compression_type(filename)

        if not compression_type:
            return None

        try:
            if compression_type == 'gzip':
                # Gzip is single-file compression
                return {
                    'type': 'gzip',
                    'file_count': 1,
                    'total_size': os.path.getsize(input_path),
                    'files': [Path(filename).stem]
                }

            elif compression_type == 'zip':
                with zipfile.ZipFile(input_path, 'r') as zip_ref:
                    file_list = zip_ref.namelist()
                    total_size = sum(info.file_size for info in zip_ref.infolist())
                    return {
                        'type': 'zip',
                        'file_count': len(file_list),
                        'total_size': total_size,
                        'files': file_list[:20]  # First 20 files only
                    }

            elif compression_type in ['tar', 'tar.gz', 'tar.bz2', 'tar.xz']:
                with tarfile.open(input_path, 'r:*') as tar_ref:
                    members = tar_ref.getmembers()
                    file_members = [m for m in members if m.isfile()]
                    total_size = sum(m.size for m in file_members)
                    return {
                        'type': compression_type,
                        'file_count': len(file_members),
                        'total_size': total_size,
                        'files': [m.name for m in file_members[:20]]  # First 20 files only
                    }

        except Exception as e:
            logger.error(f"Failed to get archive info for {input_path}: {e}")
            return None


# Convenience functions
def decompress_file(input_path: str, output_dir: str) -> Tuple[bool, List[str], Optional[str]]:
    """Convenience wrapper for CompressionHandler.decompress"""
    return CompressionHandler.decompress(input_path, output_dir)


def is_compressed_file(filename: str) -> bool:
    """Convenience wrapper for CompressionHandler.is_compressed"""
    return CompressionHandler.is_compressed(filename)


def get_archive_info(input_path: str) -> Optional[dict]:
    """Convenience wrapper for CompressionHandler.get_archive_info"""
    return CompressionHandler.get_archive_info(input_path)
