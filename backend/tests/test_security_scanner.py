# backend/tests/test_security_scanner.py
import os
import unittest
from unittest.mock import patch, MagicMock
import zipfile # For creating test zip files and ZipInfo objects
import tempfile # For creating temporary files for testing

# Assuming security_scanner is in backend directory and tests are run from root
from backend.security_scanner import _check_magic_numbers, _check_zip_bomb, \
       DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE, DEFAULT_MAX_FILE_COUNT_IN_ZIP, \
       DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE, DEFAULT_MAX_COMPRESSION_RATIO_TOTAL, \
       LXML_AVAILABLE # Import LXML_AVAILABLE for conditional skipping

# Import scan_file and _check_xxe_xml for new tests
from backend import security_scanner # To access module members via self.module_under_test


class TestSecurityScannerHelpers(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        # Allows calling functions like self.module_under_test._check_xxe_xml
        cls.module_under_test = security_scanner

    def test_check_magic_numbers_zip(self):
        with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as tmp_zip:
            with zipfile.ZipFile(tmp_zip, 'w') as zf:
                zf.writestr("test.txt", "data")
            tmp_zip_path = tmp_zip.name

        report = _check_magic_numbers(tmp_zip_path, "myfile.zip")
        self.assertEqual(report["guessed_type_from_extension"], "zip")
        self.assertEqual(report["detected_signature_type"], "zip")
        self.assertEqual(report["match_status"], "match")
        os.remove(tmp_zip_path)

    def test_check_magic_numbers_xml(self):
        with tempfile.NamedTemporaryFile(suffix=".xml", delete=False, mode="w") as tmp_xml:
            tmp_xml.write("<?xml version='1.0'?><root></root>")
            tmp_xml_path = tmp_xml.name

        report = _check_magic_numbers(tmp_xml_path, "myfile.kml") # Test with .kml ext
        self.assertEqual(report["guessed_type_from_extension"], "xml") # kml is in xml_ext list
        self.assertEqual(report["detected_signature_type"], "xml")
        self.assertEqual(report["match_status"], "match")
        os.remove(tmp_xml_path)

    def test_check_magic_numbers_mismatch(self):
        with tempfile.NamedTemporaryFile(suffix=".zip", delete=False, mode="w") as tmp_txt_as_zip:
            tmp_txt_as_zip.write("This is plain text, not a zip.") # Content doesn't start with PK
            tmp_txt_path = tmp_txt_as_zip.name

        report = _check_magic_numbers(tmp_txt_path, "myfile.zip")
        self.assertEqual(report["guessed_type_from_extension"], "zip")
        self.assertEqual(report["detected_signature_type"], "unknown") # or specific non-match
        self.assertEqual(report["match_status"], "no_match")
        os.remove(tmp_txt_path)

    def test_check_magic_numbers_unknown_extension_known_sig(self):
        with tempfile.NamedTemporaryFile(suffix=".dat", delete=False) as tmp_dat_is_zip:
            with zipfile.ZipFile(tmp_dat_is_zip, 'w') as zf:
                zf.writestr("test.txt", "data")
            tmp_dat_path = tmp_dat_is_zip.name
        report = _check_magic_numbers(tmp_dat_path, "myfile.dat")
        self.assertEqual(report["guessed_type_from_extension"], "unknown")
        self.assertEqual(report["detected_signature_type"], "zip")
        self.assertEqual(report["match_status"], "mismatch_extension") # Guessed unknown, but found zip
        os.remove(tmp_dat_path)


    # --- Tests for _check_zip_bomb ---

    @patch('zipfile.ZipFile')
    def test_zip_bomb_safe_file(self, MockZipFile):
        mock_zf_instance = MagicMock()
        mock_zip_info1 = MagicMock(spec=zipfile.ZipInfo)
        mock_zip_info1.filename = "file1.txt"; mock_zip_info1.file_size = 1000; mock_zip_info1.compress_size = 500
        mock_zip_info2 = MagicMock(spec=zipfile.ZipInfo)
        mock_zip_info2.filename = "file2.txt"; mock_zip_info2.file_size = 2000; mock_zip_info2.compress_size = 1000
        mock_zf_instance.infolist.return_value = [mock_zip_info1, mock_zip_info2]
        MockZipFile.return_value.__enter__.return_value = mock_zf_instance

        report = _check_zip_bomb("dummy.zip", DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE, DEFAULT_MAX_FILE_COUNT_IN_ZIP, DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE, DEFAULT_MAX_COMPRESSION_RATIO_TOTAL)
        self.assertFalse(report["is_bomb"])
        self.assertEqual(report["file_count"], 2)
        self.assertEqual(report["total_uncompressed_size"], 3000)
        self.assertEqual(report["total_compressed_size"], 1500)
        self.assertAlmostEqual(report["max_observed_file_ratio"], 2.0)
        self.assertAlmostEqual(report["calculated_total_ratio"], 2.0)

    @patch('zipfile.ZipFile')
    def test_zip_bomb_too_many_files(self, MockZipFile):
        mock_zf_instance = MagicMock()
        mock_infos = [MagicMock(spec=zipfile.ZipInfo) for _ in range(DEFAULT_MAX_FILE_COUNT_IN_ZIP + 1)]
        for i, mi in enumerate(mock_infos): mi.filename=f"f{i}"; mi.file_size=10; mi.compress_size=5
        mock_zf_instance.infolist.return_value = mock_infos
        MockZipFile.return_value.__enter__.return_value = mock_zf_instance
        report = _check_zip_bomb("dummy.zip", DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE, DEFAULT_MAX_FILE_COUNT_IN_ZIP, DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE, DEFAULT_MAX_COMPRESSION_RATIO_TOTAL)
        self.assertTrue(report["is_bomb"])
        self.assertIn("File count", report["reason"])

    @patch('zipfile.ZipFile')
    def test_zip_bomb_total_uncompressed_size_exceeded(self, MockZipFile):
        mock_zf_instance = MagicMock()
        mock_zip_info1 = MagicMock(spec=zipfile.ZipInfo); mock_zip_info1.filename="f1"; mock_zip_info1.file_size = DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE // 2; mock_zip_info1.compress_size = 100
        mock_zip_info2 = MagicMock(spec=zipfile.ZipInfo); mock_zip_info2.filename="f2"; mock_zip_info2.file_size = (DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE // 2) + 1; mock_zip_info2.compress_size = 100
        mock_zf_instance.infolist.return_value = [mock_zip_info1, mock_zip_info2]
        MockZipFile.return_value.__enter__.return_value = mock_zf_instance
        report = _check_zip_bomb("dummy.zip", DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE, DEFAULT_MAX_FILE_COUNT_IN_ZIP, DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE, DEFAULT_MAX_COMPRESSION_RATIO_TOTAL)
        self.assertTrue(report["is_bomb"])
        self.assertIn("Total uncompressed size", report["reason"])

    @patch('zipfile.ZipFile')
    def test_zip_bomb_single_file_ratio_exceeded(self, MockZipFile):
        mock_zf_instance = MagicMock()
        mock_zip_info1 = MagicMock(spec=zipfile.ZipInfo); mock_zip_info1.filename="f1"; mock_zip_info1.file_size = (DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE + 1) * 100; mock_zip_info1.compress_size = 100
        mock_zf_instance.infolist.return_value = [mock_zip_info1]
        MockZipFile.return_value.__enter__.return_value = mock_zf_instance
        report = _check_zip_bomb("dummy.zip", DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE, DEFAULT_MAX_FILE_COUNT_IN_ZIP, DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE, DEFAULT_MAX_COMPRESSION_RATIO_TOTAL)
        self.assertTrue(report["is_bomb"])
        self.assertIn("compression ratio", report["reason"]); self.assertIn("exceeds limit", report["reason"])

    @patch('zipfile.ZipFile')
    def test_zip_bomb_total_ratio_exceeded(self, MockZipFile):
        mock_zf_instance = MagicMock()
        mock_zip_info1 = MagicMock(spec=zipfile.ZipInfo); mock_zip_info1.filename="f1"; mock_zip_info1.file_size = 1000; mock_zip_info1.compress_size = 5
        mock_zip_info2 = MagicMock(spec=zipfile.ZipInfo); mock_zip_info2.filename="f2"; mock_zip_info2.file_size = 1000; mock_zip_info2.compress_size = 5
        mock_zf_instance.infolist.return_value = [mock_zip_info1, mock_zip_info2]
        MockZipFile.return_value.__enter__.return_value = mock_zf_instance
        report = _check_zip_bomb("dummy.zip", DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE, DEFAULT_MAX_FILE_COUNT_IN_ZIP, DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE, 100.0)
        self.assertTrue(report["is_bomb"])
        self.assertIn("Overall archive compression ratio", report["reason"])

    def test_zip_bomb_bad_zip_file(self):
        with tempfile.NamedTemporaryFile(delete=False) as tmp_bad_zip:
            tmp_bad_zip.write(b"this is not a zip file")
            tmp_bad_zip_path = tmp_bad_zip.name
        report = _check_zip_bomb(tmp_bad_zip_path, DEFAULT_MAX_TOTAL_UNCOMPRESSED_SIZE, DEFAULT_MAX_FILE_COUNT_IN_ZIP, DEFAULT_MAX_COMPRESSION_RATIO_SINGLE_FILE, DEFAULT_MAX_COMPRESSION_RATIO_TOTAL)
        self.assertTrue(report["is_bomb"]); self.assertEqual(report["error"], "BadZipFile")
        os.remove(tmp_bad_zip_path)

    # --- Tests for _check_xxe_xml ---
    @unittest.skipUnless(LXML_AVAILABLE, "lxml library not available, skipping XXE tests.")
    def test_xxe_check_safe_xml(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".xml", delete=False) as tmp_xml:
            tmp_xml.write("<?xml version='1.0'?><root><text>Hello</text></root>")
            tmp_xml_path = tmp_xml.name
        report = self.module_under_test._check_xxe_xml(tmp_xml_path)
        self.assertTrue(report["xxe_safe_parse_attempted"])
        self.assertTrue(report["parsed_successfully"])
        self.assertIsNone(report["error_message"])
        os.remove(tmp_xml_path)

    @unittest.skipUnless(LXML_AVAILABLE, "lxml library not available, skipping XXE tests.")
    def test_xxe_check_xml_with_dtd_no_external_entities(self):
        xml_content = '<!DOCTYPE foo [ <!ELEMENT foo ANY > ]><foo>bar</foo>'
        with tempfile.NamedTemporaryFile(mode="w", suffix=".xml", delete=False) as tmp_xml:
            tmp_xml.write(xml_content)
            tmp_xml_path = tmp_xml.name
        report = self.module_under_test._check_xxe_xml(tmp_xml_path)
        self.assertTrue(report["parsed_successfully"])
        self.assertIsNone(report["error_message"])
        os.remove(tmp_xml_path)

    @unittest.skipUnless(LXML_AVAILABLE, "lxml library not available, skipping XXE tests.")
    def test_xxe_check_xml_with_potential_xxe_dtd_safely_parsed(self):
        xml_content = '<!DOCTYPE foo [ <!ENTITY xxe SYSTEM "file:///etc/passwd"> ]><foo>&xxe;</foo>'
        with tempfile.NamedTemporaryFile(mode="w", suffix=".xml", delete=False) as tmp_xml:
            tmp_xml.write(xml_content)
            tmp_xml_path = tmp_xml.name
        report = self.module_under_test._check_xxe_xml(tmp_xml_path)
        self.assertTrue(report["xxe_safe_parse_attempted"])
        self.assertFalse(report["parsed_successfully"])
        self.assertIn("Entity 'xxe' not defined", report["error_message"] if report["error_message"] else "")
        os.remove(tmp_xml_path)

    @unittest.skipUnless(LXML_AVAILABLE, "lxml library not available, skipping XXE tests.")
    def test_xxe_check_malformed_xml(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".xml", delete=False) as tmp_xml:
            tmp_xml.write("<?xml version='1.0'?><root><text>Hello</text></roo>") # Malformed
            tmp_xml_path = tmp_xml.name
        report = self.module_under_test._check_xxe_xml(tmp_xml_path)
        self.assertTrue(report["xxe_safe_parse_attempted"])
        self.assertFalse(report["parsed_successfully"])
        self.assertIsNotNone(report["error_message"])
        os.remove(tmp_xml_path)

    # --- Tests for scan_file (orchestrator) ---
    @patch('backend.security_scanner._check_magic_numbers')
    @patch('backend.security_scanner._check_zip_bomb')
    @patch('backend.security_scanner._check_xxe_xml')
    def test_scan_file_orchestration_safe_zip(self, mock_xxe, mock_zip, mock_magic):
        mock_magic.return_value = {"detected_signature_type": "zip", "match_status": "match", "guessed_type_from_extension": "zip"}
        mock_zip.return_value = {"is_bomb": False, "reason": "", "file_count": 1, "error": None}

        # Need a real file for os.path.exists in scan_file
        with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as tmp_f:
            tmp_f_path = tmp_f.name

        report = self.module_under_test.scan_file(tmp_f_path, "dummy.zip")
        os.remove(tmp_f_path)

        mock_magic.assert_called_once_with(tmp_f_path, "dummy.zip")
        mock_zip.assert_called_once()
        mock_xxe.assert_not_called()
        self.assertEqual(report["scan_status"], "success")
        self.assertEqual(len(report["threats_found"]), 0)
        self.assertIsNotNone(report["zip_bomb_report"])

    @patch('backend.security_scanner._check_magic_numbers')
    @patch('backend.security_scanner._check_zip_bomb')
    @patch('backend.security_scanner._check_xxe_xml')
    def test_scan_file_orchestration_zip_bomb_detected(self, mock_xxe, mock_zip, mock_magic):
        mock_magic.return_value = {"detected_signature_type": "zip", "match_status": "match", "guessed_type_from_extension": "zip"}
        mock_zip.return_value = {"is_bomb": True, "reason": "Too many files", "error": None}

        with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as tmp_f:
            tmp_f_path = tmp_f.name

        report = self.module_under_test.scan_file(tmp_f_path, "dummy_bomb.zip")
        os.remove(tmp_f_path)

        mock_magic.assert_called_once()
        mock_zip.assert_called_once()
        mock_xxe.assert_not_called()
        self.assertEqual(report["scan_status"], "failure_threat_detected")
        self.assertEqual(report["threats_found"][0]["type"], "zip_bomb")

    @unittest.skipUnless(LXML_AVAILABLE, "lxml library not available")
    @patch('backend.security_scanner._check_magic_numbers')
    @patch('backend.security_scanner._check_zip_bomb')
    @patch('backend.security_scanner._check_xxe_xml')
    def test_scan_file_orchestration_safe_xml(self, mock_xxe, mock_zip, mock_magic):
        mock_magic.return_value = {"detected_signature_type": "xml", "match_status": "match", "guessed_type_from_extension": "xml"}
        mock_xxe.return_value = {"parsed_successfully": True, "error_message": None, "lxml_available": True, "xxe_safe_parse_attempted": True}

        with tempfile.NamedTemporaryFile(mode="w", suffix=".xml", delete=False) as tmp_xml:
            tmp_xml.write("<root/>")
            tmp_xml_path = tmp_xml.name

        report = self.module_under_test.scan_file(tmp_xml_path, "dummy.xml")
        os.remove(tmp_xml_path)

        mock_magic.assert_called_once()
        mock_zip.assert_not_called()
        mock_xxe.assert_called_once()
        self.assertEqual(report["scan_status"], "success")
        self.assertIsNotNone(report["xxe_scan_report"])

    def test_scan_file_file_not_found(self):
        report = self.module_under_test.scan_file("non_existent_file.txt", "non_existent_file.txt")
        self.assertEqual(report["scan_status"], "failure_processing_error")
        self.assertIn("File not found", report["errors"][0])

if __name__ == '__main__':
    unittest.main()
