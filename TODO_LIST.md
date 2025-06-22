# TODO and PLACEHOLDER Items

## TODOs

- `SPINDEX/core/ingestion_pipeline.c:421`: Call Geocoding Module [AEGIS-007] with extracted_locs[i] or the full list.
- `SPINDEX/core/main.c:18`: Consider making log path and level configurable via SpindexConfig
- `SPINDEX/core/test_complex_geometry_handler.c:282`: Add more tests (edge cases, malformed JSON, etc.)
- `SPINDEX/core/test_geometric_validator.c:199`: Add more tests
- `SPINDEX/core/threading.h:13`: Consider implementing a maximum queue size for TaskQueue to prevent excessive memory usage under high load.
- `SPINDEX/include/search_orchestrator.h:19`: Add other query parameters as needed, e.g., date ranges, filters
- `SPINDEX/include/search_orchestrator.h:29`: Add other result fields as needed
- `SPINDEX/plugins/aegis_modules/complex_geometry_handler.c:42`: Implement logic based on design
- `SPINDEX/plugins/classifiers/openvino_classifier.c:212`: Handle models with multiple inputs if necessary.
- `SPINDEX/plugins/classifiers/openvino_classifier.c:239`: Implement proper preprocessing (resize, pad, convert type, etc.)
- `SPINDEX/plugins/classifiers/openvino_classifier.c:267`: Handle models with multiple outputs if necessary.
- `SPINDEX/plugins/classifiers/openvino_classifier.c:324`: Populate (*output_data_ptr)->metadata if needed (e.g., classification scores, types)
- `SPINDEX/plugins/classifiers/openvino_classifier.c:335`: General TODO
- `SPINDEX/plugins/parsers/csv_parser.c:126`: trim whitespace from header token
- `SPINDEX/plugins/parsers/csv_parser.c:177`: realloc json_buffer
- `SPINDEX/plugins/parsers/csv_parser.c:182`: realloc json_buffer
- `SPINDEX/plugins/parsers/csv_parser.c:192`: realloc json_buffer
- `SPINDEX/plugins/parsers/csv_parser.c:198`: trim whitespace from headers[field_idx]
- `SPINDEX/plugins/parsers/csv_parser.c:199`: trim whitespace from current_field_value
- `SPINDEX/plugins/parsers/csv_parser.c:236`: realloc json_buffer
- `SPINDEX/plugins/parsers/csv_parser.c:241`: realloc json_buffer
- `SPINDEX/plugins/search_algorithms/avx512_phrase_search.c:195`: The plugin_interface.h currently only supports one input chunk for process_data.
- `backend/pipeline_logic.py:249`: Pass content_type if available on job model
- `backend/tests/test_index_metadata_api.py:140`: Add tests for pagination in GET /indexes if implemented in endpoint
- `backend/tests/test_index_metadata_api.py:141`: Add tests for other auth scenarios (e.g., no token, invalid token, insufficient permissions if RBAC is granular)
- `gui/script.js:403`: Add logic for dynamic index-specific params based on indexTypeSelect.value
- `gui/script.js:493`: Populate wizardAttributesSelect based on actual attributes from data (requires backend to provide this)
- `gui/script.js:574`: Collect other params based on index type
- `gui/script.js:933`: Populate specific param inputs from item.request_data.parameters

## PLACEHOLDERs

NOTICE:PLACEHOLDERS IN TESTS CAN BE IGNORED,THEY ARE TESTS AND DO NOT NEED REAL VALUES PUTTING IN UNLESS THE TEST NEEDS A REAL VALUE TO FUNCTION

- `SPINDEX/core/test_ingestion_pipeline.c:85`: `CHUNK_TYPE_PARSED_JSON_PLACEHOLDER`
- `SPINDEX/include/data_types.h:18`: `CHUNK_TYPE_PARSED_JSON_PLACEHOLDER`
- `SPINDEX/plugins/parsers/json_parser.c:96`: `CHUNK_TYPE_PARSED_JSON_PLACEHOLDER`
- `backend/main.py:163`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/main.py:164`: `SECURE_PLACEHOLDER_TOKEN_ANALYST`
- `backend/main.py:165`: `SECURE_PLACEHOLDER_TOKEN_VIEWER`
- `backend/main.py:167`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/main.py:168`: `SECURE_PLACEHOLDER_TOKEN_ANALYST`
- `backend/main.py:169`: `SECURE_PLACEHOLDER_TOKEN_VIEWER`
- `backend/tests/test_index_metadata_api.py:10`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_index_metadata_api.py:14`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:10`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`, `SECURE_PLACEHOLDER_TOKEN_ANALYST`
- `backend/tests/test_query_history_api.py:33`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:40`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:56`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:61`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:67`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:68`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:70`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:82`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:83`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:86`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:89`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:97`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:105`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:108`: `SECURE_PLACEHOLDER_TOKEN_ANALYST`
- `backend/tests/test_query_history_api.py:111`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`
- `backend/tests/test_query_history_api.py:119`: `SECURE_PLACEHOLDER_TOKEN_ANALYST`
- `docs/AEGIS_PIPELINE_TESTING_PLAN.md:34`: `SECURE_PLACEHOLDER_TOKEN_ADMIN`, `SECURE_PLACEHOLDER_TOKEN_ANALYST`
- `docs/AEGIS_PIPELINE_TESTING_PLAN.md:157`: `SECURE_PLACEHOLDER_TOKEN_VIEWER`
- `docs/AEGIS_PIPELINE_TESTING_PLAN.md:162`: `SECURE_PLACEHOLDER_TOKEN_VIEWER`
