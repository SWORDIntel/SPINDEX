# backend/security_scanner.py
import os
import zipfile
import logging
from typing import Dict, Any, Optional, List, Tuple

try:
    from lxml import etree
    LXML_AVAILABLE = True
except ImportError:
    LXML_AVAILABLE = False
    # Fallback or warning if lxml is not available, XXE check will be skipped/degraded.
    # This should ideally not happen if requirements.txt is used.

logger = logging.getLogger(__name__)

# --- Configuration Constants ---
DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE = 1 * 1024 * 1024 * 1024  # 1 GB
DEFAULT_MAX_FILE_COUNT_IN_ZIP = 10000
DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE = 200.0
DEFAULT_MAX_COMPRESSION_RATIO_TOTAL = 100.0

# --- Helper Functions ---

def _check_magic_numbers(file_path: str, original_filename: str) -> Dict[str, Any]:
    report = {
        "file_path": file_path,
        "guessed_type_from_extension": "unknown",
        "detected_signature_type": "unknown",
        "match_status": "indeterminate",
        "details": ""
    }
    file_ext = original_filename.lower().split('.')[-1] if '.' in original_filename else ""

    type_map = {
        "zip": {"ext": ["zip"], "magic": [b'PK\x03\x04']},
        "xml": {"ext": ["xml", "kml", "gml"], "magic": [b'<?xml', b'<kml', b'<gml']} # Common starts
        # Add more types as needed
    }

    guessed_category = "unknown"
    if file_ext in type_map["zip"]["ext"]:
        guessed_category = "zip"
        report["guessed_type_from_extension"] = "zip"
    elif file_ext in type_map["xml"]["ext"]:
        guessed_category = "xml"
        report["guessed_type_from_extension"] = "xml"

    try:
        with open(file_path, 'rb') as f:
            file_start_bytes = f.read(100) # Read first 100 bytes

        if guessed_category == "zip" and any(file_start_bytes.startswith(sig) for sig in type_map["zip"]["magic"]):
            report["detected_signature_type"] = "zip"
            report["match_status"] = "match"
        elif guessed_category == "xml" and any(file_start_bytes.startswith(sig) for sig in type_map["xml"]["magic"]):
            report["detected_signature_type"] = "xml"
            report["match_status"] = "match"
        elif any(file_start_bytes.startswith(sig) for sig in type_map["zip"]["magic"]): # Check even if ext mismatch
            report["detected_signature_type"] = "zip"
            report["match_status"] = "mismatch_extension" if guessed_category != "zip" else "match"
        elif any(file_start_bytes.startswith(sig) for sig in type_map["xml"]["magic"]):
            report["detected_signature_type"] = "xml"
            report["match_status"] = "mismatch_extension" if guessed_category != "xml" else "match"
        else:
            report["details"] = "File signature did not match common ZIP or XML patterns."
            report["match_status"] = "no_match" if guessed_category != "unknown" else "unknown_signature"

    except IOError as e:
        report["details"] = f"IOError reading file for magic number check: {e}"
        report["match_status"] = "error_reading_file"

    return report

def _check_zip_bomb(file_path: str,
                    max_total_uncompressed_size: int,
                    max_files: int,
                    max_ratio_file: float,
                    max_ratio_total: float) -> Dict[str, Any]:
    report = {
        "is_bomb": False,
        "reason": "",
        "total_uncompressed_size": 0,
        "total_compressed_size": 0,
        "file_count": 0,
        "max_observed_file_ratio": 0.0,
        "calculated_total_ratio": 0.0,
        "error": None
    }
    try:
        with zipfile.ZipFile(file_path, 'r') as zf:
            zip_infos = zf.infolist()
            report["file_count"] = len(zip_infos)

            if report["file_count"] > max_files:
                report["is_bomb"] = True
                report["reason"] = f"File count {report['file_count']} exceeds limit {max_files}."
                return report # Early exit

            for zip_info in zip_infos:
                report["total_uncompressed_size"] += zip_info.file_size
                report["total_compressed_size"] += zip_info.compress_size

                if zip_info.compress_size > 0:
                    ratio = zip_info.file_size / zip_info.compress_size
                    if ratio > report["max_observed_file_ratio"]:
                        report["max_observed_file_ratio"] = ratio
                    if ratio > max_ratio_file:
                        report["is_bomb"] = True
                        report["reason"] = f"File '{zip_info.filename}' compression ratio {ratio:.2f} exceeds limit {max_ratio_file}."
                        return report # Early exit

            if report["total_uncompressed_size"] > max_total_uncompressed_size:
                report["is_bomb"] = True
                report["reason"] = f"Total uncompressed size {report['total_uncompressed_size']} bytes exceeds limit {max_total_uncompressed_size} bytes."
                return report

            if report["total_compressed_size"] > 0:
                report["calculated_total_ratio"] = report["total_uncompressed_size"] / report["total_compressed_size"]
                if report["calculated_total_ratio"] > max_ratio_total:
                    report["is_bomb"] = True
                    report["reason"] = f"Overall archive compression ratio {report['calculated_total_ratio']:.2f} exceeds limit {max_ratio_total}."
                    return report

    except zipfile.BadZipFile:
        report["is_bomb"] = True # Treat as a threat or processing error
        report["reason"] = "File is not a valid ZIP archive or is corrupted."
        report["error"] = "BadZipFile"
    except Exception as e:
        logger.error(f"Error checking zip bomb for {file_path}: {e}", exc_info=True)
        report["error"] = str(e)
        # Depending on policy, an error during check might be considered a threat
        # report["is_bomb"] = True
        # report["reason"] = f"Error during zip bomb check: {e}"
    return report

def _check_xxe_xml(file_path: str) -> Dict[str, Any]:
    report = {
        "xxe_safe_parse_attempted": False,
        "parsed_successfully": False,
        "error_message": None,
        "lxml_available": LXML_AVAILABLE
    }
    if not LXML_AVAILABLE:
        report["error_message"] = "LXML library not available. XXE check skipped."
        return report

    report["xxe_safe_parse_attempted"] = True
    try:
        # Secure parser configuration for lxml
        # Explicitly disable network access for DTDs and entities
        parser = etree.XMLParser(resolve_entities=False, no_network=True)
        etree.parse(file_path, parser)
        report["parsed_successfully"] = True
    except etree.XMLSyntaxError as e:
        report["error_message"] = f"XMLSyntaxError: {e}"
    except Exception as e: # Catch other potential lxml or file errors
        logger.error(f"Error parsing XML for XXE check {file_path}: {e}", exc_info=True)
        report["error_message"] = f"General parsing error: {e}"
    return report

# --- Main Public Function ---
def scan_file(file_path: str,
                original_filename: str,
                content_type: Optional[str] = None,
                # detected_format_by_gdal: Optional[str] = None, # Not used yet, but could inform which scans to run
                config: Optional[Dict[str, Any]] = None # For passing custom thresholds
            ) -> Dict[str, Any]:

    if config is None: config = {}
    max_total_size = config.get("max_total_uncompressed_size", DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE)
    max_files = config.get("max_file_count_in_zip", DEFAULT_MAX_FILE_COUNT_IN_ZIP)
    max_ratio_file = config.get("max_compression_ratio_single_file", DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE)
    max_ratio_total = config.get("max_compression_ratio_total", DEFAULT_MAX_COMPRESSION_RATIO_TOTAL)

    results: Dict[str, Any] = {
        "scan_status": "success",
        "threats_found": [],
        "file_type_validation_report": None,
        "zip_bomb_report": None,
        "xxe_scan_report": None,
        "errors": []
    }

    if not os.path.exists(file_path):
        results["scan_status"] = "failure_processing_error"
        results["errors"].append(f"File not found at path: {file_path}")
        return results

    # 1. Magic Number Check
    results["file_type_validation_report"] = _check_magic_numbers(file_path, original_filename)
    detected_sig_type = results["file_type_validation_report"].get("detected_signature_type", "unknown")

    file_ext_lower = original_filename.lower().split('.')[-1] if '.' in original_filename else ""

    # 2. Zip Bomb Check (if extension or signature suggests ZIP)
    is_zip_by_ext = file_ext_lower == "zip"
    is_zip_by_sig = detected_sig_type == "zip"

    if is_zip_by_ext or is_zip_by_sig:
        if not is_zip_by_sig and is_zip_by_ext : # Extension is zip, but magic mismatch
            logger.warning(f"File {original_filename} has .zip extension but non-zip magic number. Attempting zip bomb check cautiously.")

        zip_report = _check_zip_bomb(file_path, max_total_size, max_files, max_ratio_file, max_ratio_total)
        results["zip_bomb_report"] = zip_report
        if zip_report.get("is_bomb"):
            results["threats_found"].append({"type": "zip_bomb", "detail": zip_report.get("reason")})
            results["scan_status"] = "failure_threat_detected"
            # If it's a zip bomb, no further checks needed on this file itself.
            return results
        if zip_report.get("error"):
            results["errors"].append(f"Error during zip bomb scan: {zip_report.get('error')}")
            results["scan_status"] = "failure_processing_error" # If check itself fails critically
            return results


    # 3. XXE Check (if extension or signature suggests XML and not a zip bomb)
    is_xml_by_ext = file_ext_lower in ["xml", "kml", "gml"] # Add more as needed
    is_xml_by_sig = detected_sig_type == "xml"

    # Only run XXE if it's likely XML AND not a ZIP (or if it's an XML file extracted from a non-bomb ZIP - future enhancement)
    # For now, if it was identified and processed as a ZIP above, we don't also XXE scan the .zip file itself.
    if not (is_zip_by_ext or is_zip_by_sig): # If it wasn't treated as a zip
        if is_xml_by_ext or is_xml_by_sig:
            if not LXML_AVAILABLE:
                logger.warning("LXML not available, XXE scan for {original_filename} will be skipped.")
                results["xxe_scan_report"] = {"lxml_available": False, "error_message": "LXML library not available."}
                # Potentially make this a processing error or a less severe status
                # results["scan_status"] = "warning_scanner_degraded"
            else:
                xxe_report = _check_xxe_xml(file_path)
                results["xxe_scan_report"] = xxe_report
                if not xxe_report.get("parsed_successfully") and xxe_report.get("error_message"):
                    # A simple XML syntax error isn't an XXE attack succeeding.
                    # However, if strict well-formed XML is required, this could be a processing failure.
                    logger.warning(f"XML parsing failed for {original_filename}: {xxe_report.get('error_message')}")
                    # For now, log it. Deciding if this is a "threat" is nuanced.
                    # If the parser was secure, XXE itself was prevented.
                    # results["errors"].append(f"XML parse error: {xxe_report.get('error_message')}")
                    # Potentially: results["scan_status"] = "failure_processing_error" if XML must be valid

    if results["threats_found"]: # Re-check in case any other scan type adds threats
        results["scan_status"] = "failure_threat_detected"
    elif results["errors"]:
        results["scan_status"] = "failure_processing_error"

    return results

if __name__ == '__main__':
    # Basic test examples (requires creating dummy files)
    logging.basicConfig(level=logging.INFO)

    # Create a dummy safe XML file
    safe_xml_path = "safe_test.xml"
    with open(safe_xml_path, "w") as f:
        f.write("<?xml version='1.0'?><root><element>Test</element></root>")

    logger.info(f"--- Scanning {safe_xml_path} ---")
    report_safe_xml = scan_file(safe_xml_path, safe_xml_path)
    logger.info(json.dumps(report_safe_xml, indent=2))
    os.remove(safe_xml_path)

    # Create a dummy (non-bomb) zip file
    safe_zip_path = "safe_test.zip"
    with zipfile.ZipFile(safe_zip_path, 'w') as zf:
        zf.writestr("file1.txt", "This is a test file.")
        zf.writestr("file2.txt", "Another test file.")

    logger.info(f"--- Scanning {safe_zip_path} ---")
    report_safe_zip = scan_file(safe_zip_path, safe_zip_path)
    logger.info(json.dumps(report_safe_zip, indent=2))
    os.remove(safe_zip_path)

    if LXML_AVAILABLE:
        # Create a dummy XML file with an external entity declaration (for XXE potential test)
        # This file itself is not harmful if parsed with a secure parser.
        xxe_potential_path = "xxe_potential_test.xml"
        with open(xxe_potential_path, "w") as f:
            f.write('<?xml version="1.0"?><!DOCTYPE foo [ <!ENTITY xxe SYSTEM "file:///etc/passwd"> ]><foo>&xxe;</foo>')
        logger.info(f"--- Scanning {xxe_potential_path} (XXE potential) ---")
        report_xxe = scan_file(xxe_potential_path, xxe_potential_path)
        logger.info(json.dumps(report_xxe, indent=2))
        os.remove(xxe_potential_path)
    else:
        logger.warning("Skipping XXE potential test as LXML is not available.")
