document.addEventListener('DOMContentLoaded', () => {
    const views = document.querySelectorAll('.view');
    const navLinks = document.querySelectorAll('#primary-nav ul li a');
    const sidebar = document.getElementById('sidebar');
    const sidebarToggle = document.getElementById('sidebar-toggle');

    // --- User Info & Auth Stubs ---
    const userInfoDisplay = document.getElementById('user-info');
    const loginButton = document.getElementById('login-button');
    const logoutButton = document.getElementById('logout-button');
    let currentUser = { username: 'Guest', role: 'guest', token: null }; // Placeholder

    function updateUserInfoDisplay() {
        if (userInfoDisplay) {
            if (currentUser.username && currentUser.username !== 'Guest') {
                userInfoDisplay.textContent = `${currentUser.username} (${currentUser.role})`;
                if (loginButton) loginButton.style.display = 'none';
                if (logoutButton) logoutButton.style.display = 'inline';
            } else {
                userInfoDisplay.textContent = 'Guest';
                if (loginButton) loginButton.style.display = 'inline';
                if (logoutButton) logoutButton.style.display = 'none';
            }
        }
    }

    if (loginButton) {
        loginButton.addEventListener('click', () => {
            // Dummy login
            currentUser.username = 'testuser';
            currentUser.role = 'analyst';
            currentUser.token = 'faketoken_admin'; // Using admin token for testing upload
            updateUserInfoDisplay();
            alert('Logged in as testuser (analyst) with admin token for testing.');
        });
    }

    if (logoutButton) {
        logoutButton.addEventListener('click', () => {
            currentUser.username = 'Guest';
            currentUser.role = 'guest';
            currentUser.token = null;
            updateUserInfoDisplay();
            alert('Logged out.');
            showView('dashboard-view'); // Or a dedicated login view
        });
    }

    // Initial UI state
    updateUserInfoDisplay(); // Call on load to set initial state

    // --- Loading Indicator and Skeleton Helper Functions ---
    function showLoading(elementOrSelector, type = 'spinner', count = 3, itemHtml = '<div class="skeleton skeleton-text"></div>') {
        const targetElement = typeof elementOrSelector === 'string' ? document.querySelector(elementOrSelector) : elementOrSelector;
        if (!targetElement) return;

        if (type === 'spinner') {
            const spinner = document.createElement('div');
            spinner.className = 'loader';
            targetElement.innerHTML = ''; // Clear existing content
            targetElement.appendChild(spinner);
        } else if (type === 'skeleton') {
            let skeletonHtml = '';
            for (let i = 0; i < count; i++) {
                skeletonHtml += itemHtml; // Use provided itemHtml for skeleton structure
            }
            targetElement.innerHTML = skeletonHtml;
        }
    }

    function hideLoading(elementOrSelector, isEmpty = false) {
        const targetElement = typeof elementOrSelector === 'string' ? document.querySelector(elementOrSelector) : elementOrSelector;
        if (!targetElement) return;

        // Only clear if it's truly empty after loading or if explicitly told it might be (e.g. no data found)
        // Otherwise, the content has been populated by the loading function's caller.
        const spinner = targetElement.querySelector('.loader');
        if (spinner) {
            spinner.remove();
        }
        // For skeletons, the content is usually replaced by the actual data, so no specific removal needed here
        // unless the loading function itself needs to clear these (which it does by setting innerHTML).
        // If after loading, the container is meant to be empty (e.g. "No data found"), this is handled by caller.
    }


    // --- View Switching Logic ---
    function showView(viewId) {
        views.forEach(view => {
            if (view.id === viewId) {
                view.style.display = 'block'; // or view.classList.add('active-view');
            } else {
                view.style.display = 'none'; // or view.classList.remove('active-view');
            }
        });
        // Update active link in sidebar
        navLinks.forEach(link => {
            if (link.getAttribute('data-view') === viewId) {
                link.classList.add('active-nav-link'); // Add a class for styling active nav link
            } else {
                link.classList.remove('active-nav-link');
            }
        });
    }

    navLinks.forEach(link => {
        link.addEventListener('click', (event) => {
            event.preventDefault();
            const viewId = link.getAttribute('data-view');
            if (viewId) {
                 // Special handling for ingestion view if user is not logged in
                if (viewId === 'ingestion-view' && (!currentUser.token)) {
                    alert('Please login to access the Ingestion Dashboard.');
                    // Optionally, redirect to a login page or show a login modal
                    return;
                }
                showView(viewId);
            }
        });
    });

    // Set initial view (e.g., dashboard)
    // The HTML already sets dashboard-view with class "active-view", so this ensures JS matches that.
    const initialActiveView = document.querySelector('.active-view');
    if (initialActiveView) {
        showView(initialActiveView.id);
    } else {
        showView('dashboard-view'); // Default if no view is active in HTML
    }


    // --- Sidebar Toggle ---
    if (sidebar && sidebarToggle) {
        sidebarToggle.addEventListener('click', () => {
            document.body.classList.toggle('sidebar-collapsed');
            // Adjust toggle button text/icon if needed based on collapsed state
        });
    }

    // --- Ingestion Dashboard Logic ---
    const fileInput = document.getElementById('file-input');
    const uploadButton = document.getElementById('upload-button');
    const uploadStatusDiv = document.getElementById('upload-status');
    const ingestionJobsTableBody = document.getElementById('ingestion-jobs-table')?.getElementsByTagName('tbody')[0];

    function addJobToIngestionTable(job) {
        if (!ingestionJobsTableBody) return;
        const newRow = ingestionJobsTableBody.insertRow(0); // Insert at the top
        newRow.insertCell().textContent = job.job_id || 'N/A';
        newRow.insertCell().textContent = job.filename || 'N/A';
        newRow.insertCell().textContent = job.status || 'Queued'; // Initial status
        newRow.insertCell().textContent = job.upload_timestamp || new Date().toLocaleDateString();

        const actionsCell = newRow.insertCell();
        const detailsButton = document.createElement('button');
        detailsButton.textContent = 'View Details';
        detailsButton.onclick = () => {
            // Fetch job status from /tasks/status/{job.job_id}
            // Or directly use metadata endpoint: /metadata/ingestion_jobs/{job.job_id}
            alert(`Fetching details for Job ID: ${job.job_id}`);
            // For now, just log to console or show basic alert
            // In future, could show a modal with full metadata
            fetch(`/metadata/ingestion_jobs/${job.job_id}`, {
                headers: { 'Authorization': `Bearer ${currentUser.token}` }
            })
            .then(response => response.json())
            .then(data => {
                if (data.detail) { // Handle FastAPI error response
                    alert(`Error fetching job details: ${data.detail}`);
                } else {
                    alert(`Job Details:\nID: ${data.job_id}\nFile: ${data.original_filename}\nStatus: ${data.processing_status}\nUploaded: ${new Date(data.upload_timestamp).toLocaleString()}\nOutput: ${data.location_of_promoted_data || 'N/A'}`);
                    // Update table row status if it changed
                    const rowToUpdate = Array.from(ingestionJobsTableBody.rows).find(row => row.cells[0].textContent === job.job_id);
                    if(rowToUpdate) {
                        rowToUpdate.cells[2].textContent = data.processing_status;
                    }
                }
            })
            .catch(error => {
                console.error('Error fetching job details:', error);
                alert('Error fetching job details. Check console.');
            });
        };
        actionsCell.appendChild(detailsButton);
    }

    if (uploadButton && fileInput && uploadStatusDiv) {
        uploadButton.addEventListener('click', () => {
            if (!currentUser.token) {
                alert('Please login to upload files.');
                uploadStatusDiv.textContent = 'Error: Please login to upload.';
                uploadStatusDiv.className = 'upload-status-message error';
                return;
            }

            const file = fileInput.files[0];
            if (!file) {
                uploadStatusDiv.textContent = 'Please select a file to upload.';
                uploadStatusDiv.className = 'upload-status-message error';
                return;
            }

            uploadStatusDiv.textContent = 'Uploading...';
            uploadStatusDiv.className = 'upload-status-message'; // Neutral style

            const formData = new FormData();
            formData.append('file', file);

            fetch('/upload/data', { // Assuming backend is on the same origin or proxied
                method: 'POST',
                body: formData,
                headers: {
                    // 'Content-Type': 'multipart/form-data' is automatically set by browser for FormData
                    'Authorization': `Bearer ${currentUser.token}` // Use actual token
                }
            })
            .then(response => {
                if (!response.ok) {
                    return response.json().then(err => { throw new Error(err.detail || `HTTP error! status: ${response.status}`) });
                }
                return response.json();
            })
            .then(data => {
                uploadStatusDiv.textContent = `Success: ${data.message} (Job ID: ${data.job_id})`;
                uploadStatusDiv.className = 'upload-status-message success';
                fileInput.value = ''; // Clear the file input

                // Add to table (basic info from upload response)
                addJobToIngestionTable({
                    job_id: data.job_id,
                    filename: data.filename,
                    status: "Queued for Processing", // Initial status from frontend perspective
                    upload_timestamp: new Date().toISOString() // Or use server's upload_timestamp if available directly
                });
            })
            .catch(error => {
                console.error('Upload error:', error);
                uploadStatusDiv.textContent = `Error: ${error.message}`;
                uploadStatusDiv.className = 'upload-status-message error';
            });
        });
    }

    // Periodically refresh job statuses in the table (very basic poller)
    function refreshJobStatuses() {
        if (!ingestionJobsTableBody || !currentUser.token || document.getElementById('ingestion-view').style.display === 'none') {
            return; // Don't poll if table not visible or not logged in
        }
        const rows = ingestionJobsTableBody.getElementsByTagName('tr');
        for (let row of rows) {
            const jobId = row.cells[0].textContent;
            const currentStatus = row.cells[2].textContent;
            // Avoid re-fetching for already completed/failed states if desired, or always fetch
            if (jobId && jobId !== 'N/A' /* && !['validation_successful_ready_for_promotion', 'quarantined_validation_failed', 'crs_transformed_failed'].includes(currentStatus) */) {
                 fetch(`/metadata/ingestion_jobs/${jobId}`, {
                    headers: { 'Authorization': `Bearer ${currentUser.token}` }
                })
                .then(response => response.ok ? response.json() : null)
                .then(data => {
                    if (data && data.processing_status !== currentStatus) {
                        row.cells[2].textContent = data.processing_status;
                    }
                })
                .catch(error => console.error(`Error refreshing status for job ${jobId}:`, error));
            }
        }
    }
    // Poll every 10 seconds (adjust as needed)
    // setInterval(refreshJobStatuses, 10000); // Uncomment to enable polling


    // --- Quarantine Table Logic ---
    const quarantineJobsTableBody = document.getElementById('quarantine-jobs-table')?.getElementsByTagName('tbody')[0];

    function addJobToQuarantineTable(job) {
        if (!quarantineJobsTableBody) return;
        const newRow = quarantineJobsTableBody.insertRow(0); // Insert at the top
        newRow.insertCell().textContent = job.id || 'N/A';
        newRow.insertCell().textContent = job.filename || 'N/A';
        newRow.insertCell().textContent = job.quarantined_at || new Date().toLocaleDateString();
        newRow.insertCell().textContent = job.error_summary || 'Unknown error';

        const actionsCell = newRow.insertCell();
        const detailsButton = document.createElement('button');
        detailsButton.textContent = 'Details';
        detailsButton.onclick = () => viewQuarantineDetails(job.id); // Note: viewQuarantineDetails needs to be global or passed correctly
        actionsCell.appendChild(detailsButton);

        const retryButton = document.createElement('button');
        retryButton.textContent = 'Retry';
        retryButton.onclick = () => retryQuarantineJob(job.id); // Note: retryQuarantineJob needs to be global or passed correctly
        actionsCell.appendChild(retryButton);

        const discardButton = document.createElement('button');
        discardButton.textContent = 'Discard';
        discardButton.onclick = () => discardQuarantineJob(job.id); // Note: discardQuarantineJob needs to be global or passed correctly
        actionsCell.appendChild(discardButton);
    }

    // Make functions globally accessible if they are called via inline HTML onclick
    // A better approach would be to add event listeners programmatically if this script controlled all HTML generation.
    window.viewQuarantineDetails = async function(jobId) {
        const detailsDiv = document.getElementById('quarantine-job-details');
        const contentPre = document.getElementById('quarantine-details-content');
        if (!detailsDiv || !contentPre) return;

        contentPre.textContent = 'Fetching details...';
        detailsDiv.style.display = 'block';

        try {
            const response = await fetch(`/metadata/ingestion_jobs/${jobId}`, {
                headers: { 'Authorization': `Bearer ${currentUser.token || 'faketoken_admin'}` }
            });
            if (!response.ok) {
                const errorData = await response.text(); // Try to get more error info
                throw new Error(`Failed to fetch details: ${response.status} ${response.statusText}. Server says: ${errorData}`);
            }
            const jobDetails = await response.json();
            contentPre.textContent = JSON.stringify(jobDetails, null, 2);
        } catch (error) {
            console.error('Error fetching quarantine details:', error);
            contentPre.textContent = `Error fetching details: ${error.message}`;
        }
    };

    window.retryQuarantineJob = function(jobId) {
        alert(`Retry job ${jobId} - not implemented yet.`);
        // Future implementation:
        // 1. Call a backend endpoint like POST /tasks/retry/{jobId}
        // 2. Backend would then re-queue the original task or a new one with same params.
        // 3. Update UI: remove from quarantine table or mark as "retrying".
    };

    window.discardQuarantineJob = function(jobId) {
        if (confirm(`Are you sure you want to discard quarantined job ${jobId}? This action cannot be undone.`)) {
            alert(`Discard job ${jobId} - not implemented yet.`);
            // Future implementation:
            // 1. Call a backend endpoint like DELETE /metadata/ingestion_jobs/{jobId} (or a dedicated discard endpoint).
            // 2. Backend marks job as discarded, potentially cleans up staged/processed files.
            // 3. Update UI: remove from quarantine table.
            // For now, remove from UI if quarantineJobsTableBody is accessible:
            if (quarantineJobsTableBody) {
                const rowToRemove = Array.from(quarantineJobsTableBody.rows).find(row => row.cells[0].textContent === jobId);
                if (rowToRemove) {
                    rowToRemove.remove();
                    const detailsDiv = document.getElementById('quarantine-job-details');
                    if (detailsDiv && detailsDiv.style.display === 'block' && document.getElementById('quarantine-details-content').textContent.includes(jobId)) {
                        detailsDiv.style.display = 'none'; // Hide details if they were for this job
                    }
                }
            }
        }
    };

    // Populate Quarantine Table with dummy data for visual testing
    if (quarantineJobsTableBody) { // Ensure table exists
        addJobToQuarantineTable({
            id: 'job_q1',
            filename: 'error_file.shp',
            quarantined_at: new Date(2023, 9, 25).toLocaleDateString(), // Month is 0-indexed
            error_summary: 'Invalid geometry detected after CRS transform.'
        });
        addJobToQuarantineTable({
            id: 'job_q2',
            filename: 'corrupt_archive.zip',
            quarantined_at: new Date(2023, 9, 26).toLocaleDateString(),
            error_summary: 'Security scan: Potential zip bomb.'
        });
    }

    // --- Index Management View & Create Index Wizard Logic ---
    const indexesTableBody = document.getElementById('indexes-table')?.getElementsByTagName('tbody')[0];
    const indexDetailsArea = document.getElementById('index-details-area');
    const indexDetailsContent = document.getElementById('index-details-content');
    let currentlyDisplayedIndexId = null; // For progressive disclosure tracking
    const showCreateIndexWizardButton = document.getElementById('show-create-index-wizard-button');

    const wizardModal = document.getElementById('create-index-wizard-modal');
    const wizardSteps = wizardModal?.querySelectorAll('.wizard-step');
    let currentWizardStep = 1;
    let selectedProcessedJobData = null; // To store data of the selected promoted job

    const processedDataSelect = document.getElementById('processed-data-select');
    // Step 1 elements
    const wizardSelectedFilename = document.getElementById('wizard-selected-filename');
    const wizardSelectedJobId = document.getElementById('wizard-selected-jobid');
    const wizardSelectedFilepath = document.getElementById('wizard-selected-filepath');
    // Step 2 elements
    const wizardValOrigFormat = document.getElementById('wizard-val-orig-format');
    const wizardValOrigCRS = document.getElementById('wizard-val-orig-crs');
    const wizardValTargetCRS = document.getElementById('wizard-val-target-crs');
    const wizardValGeom = document.getElementById('wizard-val-geom');
    const wizardValSchema = document.getElementById('wizard-val-schema'); // Placeholder
    const wizardValSecurity = document.getElementById('wizard-val-security'); // Placeholder
    const wizardAttributesSelect = document.getElementById('wizard-attributes-select');
    const wizardAckValidationCheckbox = document.getElementById('wizard-ack-validation');
    const wizardStep2NextButton = document.getElementById('wizard-step2-next');

    // Step 3 elements
    const indexNameInput = document.getElementById('index-name-input');
    const indexDescriptionInput = document.getElementById('index-description-input');
    const indexTypeSelect = document.getElementById('index-type-select');
    const rtreeParamsContainer = document.getElementById('rtree-params-container');
    const rtreeMaxEntriesInput = document.getElementById('rtree-max-entries');
    const elasticsearchParamsContainer = document.getElementById('elasticsearch-params-container');
    const esShardsInput = document.getElementById('es-shards-input');
    const esReplicasInput = document.getElementById('es-replicas-input');

    if (indexTypeSelect) {
        indexTypeSelect.addEventListener('change', function() {
            if (rtreeParamsContainer) rtreeParamsContainer.style.display = this.value === 'RTree' ? 'block' : 'none';
            if (elasticsearchParamsContainer) elasticsearchParamsContainer.style.display = this.value === 'Elasticsearch' ? 'block' : 'none';
            // Add more cases for other index types
        });
        // Trigger change on load to set initial visibility
        indexTypeSelect.dispatchEvent(new Event('change'));
    }


    // Step 4 elements
    const summaryDataPath = document.getElementById('summary-data-path');
    const summaryIndexName = document.getElementById('summary-index-name');
    const summaryIndexType = document.getElementById('summary-index-type');
    const summaryIndexParams = document.getElementById('summary-index-params');
    const summaryIndexAttributes = document.getElementById('summary-index-attributes');

    async function populateProcessedDataSelect() {
        if (!processedDataSelect) return;
        showLoading(processedDataSelect.parentNode, 'spinner'); // Show spinner near select
        processedDataSelect.innerHTML = ''; // Clear old options
        const placeholderOption = document.createElement('option');
        placeholderOption.value = "";
        placeholderOption.textContent = "Loading available datasets...";
        placeholderOption.disabled = true;
        processedDataSelect.appendChild(placeholderOption);

        try {
            const response = await fetch('/metadata/ingestion_jobs?processing_status=validation_successful_ready_for_promotion', {
                headers: { 'Authorization': `Bearer ${currentUser.token || 'faketoken_admin'}` }
            });
            if (!response.ok) throw new Error(`Failed to fetch processed data: ${response.statusText}`);
            const jobs = await response.json();

            processedDataSelect.innerHTML = ''; // Clear "Loading..."
            const defaultOption = document.createElement('option');
            defaultOption.value = "";
            defaultOption.textContent = "-- Select a processed dataset --";
            processedDataSelect.appendChild(defaultOption);

            if (jobs.length === 0) {
                const noDataOption = document.createElement('option');
                noDataOption.value = "";
                noDataOption.disabled = true;
                noDataOption.textContent = "No processed and validated datasets available.";
                processedDataSelect.appendChild(noDataOption);
                hideLoading(processedDataSelect.parentNode); // Hide spinner
                return;
            }
            jobs.forEach(job => {
                const option = document.createElement('option');
                option.value = job.job_id; // Use job_id (which is celery_task_id)
                option.textContent = `${job.original_filename} (Job: ${job.job_id}, Promoted: ${new Date(job.promotion_timestamp).toLocaleDateString()})`;
                option.dataset.filePath = job.location_of_promoted_data; // Store full path
                option.dataset.originalFilename = job.original_filename;
                // Store all job data for later use
                option.dataset.fullJobData = JSON.stringify(job);
                processedDataSelect.appendChild(option);
            });
        } catch (error) {
            console.error("Error populating processed data select:", error);
            processedDataSelect.innerHTML = '<option value="">Error loading data.</option>';
        } finally {
            hideLoading(processedDataSelect.parentNode); // Hide spinner
        }
    }

    if (processedDataSelect) {
        processedDataSelect.addEventListener('change', function() {
            const selectedOption = this.options[this.selectedIndex];
            if (selectedOption && selectedOption.value) {
                selectedProcessedJobData = JSON.parse(selectedOption.dataset.fullJobData);
                if(wizardSelectedFilename) wizardSelectedFilename.textContent = selectedProcessedJobData.original_filename;
                if(wizardSelectedJobId) wizardSelectedJobId.textContent = selectedProcessedJobData.job_id;
                if(wizardSelectedFilepath) wizardSelectedFilepath.textContent = selectedProcessedJobData.location_of_promoted_data;
            } else {
                selectedProcessedJobData = null;
                if(wizardSelectedFilename) wizardSelectedFilename.textContent = '-';
                if(wizardSelectedJobId) wizardSelectedJobId.textContent = '-';
                if(wizardSelectedFilepath) wizardSelectedFilepath.textContent = '-';
            }
        });
    }

    function displayValidationSummaryForWizard() {
        if (!selectedProcessedJobData) return;
        if(wizardValOrigFormat) wizardValOrigFormat.textContent = selectedProcessedJobData.detected_format || 'N/A';
        if(wizardValOrigCRS) wizardValOrigCRS.textContent = selectedProcessedJobData.original_crs || 'N/A';
        if(wizardValTargetCRS) wizardValTargetCRS.textContent = selectedProcessedJobData.target_crs || 'N/A';

        let geomSummary = "Not available";
        if (selectedProcessedJobData.validation_summary && selectedProcessedJobData.validation_summary.geometric_validation) {
            const gv = selectedProcessedJobData.validation_summary.geometric_validation;
            geomSummary = `${gv.valid_features} valid, ${gv.invalid_features} invalid. Passed: ${gv.passed}`;
        }
        if(wizardValGeom) wizardValGeom.textContent = geomSummary;

        // TODO: Populate wizardAttributesSelect. Requires backend to provide feature attributes,
        // ideally in selectedProcessedJobData.feature_attributes (as a list of strings)
        // or via a new endpoint that returns the schema/attributes of the processed data.
        if (wizardAttributesSelect) {
            wizardAttributesSelect.innerHTML = ''; // Clear existing
            let attributes = [];
            if (selectedProcessedJobData && selectedProcessedJobData.schema_preview) { // Assuming schema_preview might exist
                // Example: selectedProcessedJobData.schema_preview = {"properties": {"name": "string", "value": "number"}}
                // This is hypothetical, adapt if backend provides attributes differently
                if (selectedProcessedJobData.schema_preview.properties) {
                    attributes = Object.keys(selectedProcessedJobData.schema_preview.properties);
                } else if (Array.isArray(selectedProcessedJobData.schema_preview)) { // Or if it's just a list of strings
                     attributes = selectedProcessedJobData.schema_preview;
                }
            }

            if (attributes.length === 0) { // Fallback if no attributes found
                attributes = ['Placeholder Property A', 'Placeholder Property B', 'geometry'];
            }

            attributes.forEach(attr => {
                const option = document.createElement('option');
                option.value = attr;
                option.textContent = attr;
                wizardAttributesSelect.appendChild(option);
            });
        }
    }

    window.openCreateIndexWizard = async function() {
        if (!currentUser.token) { alert("Please login first."); return; }
        if (!wizardModal) { console.error("Wizard modal not found"); return; }

        selectedProcessedJobData = null; // Reset
        if(wizardSelectedFilename) wizardSelectedFilename.textContent = '-';
        if(wizardSelectedJobId) wizardSelectedJobId.textContent = '-';
        if(wizardSelectedFilepath) wizardSelectedFilepath.textContent = '-';
        if(wizardAckValidationCheckbox) wizardAckValidationCheckbox.checked = false;
        if(wizardStep2NextButton) wizardStep2NextButton.disabled = true;


        await populateProcessedDataSelect();
        currentWizardStep = 1;
        wizardNavigate(1); // Show first step
        wizardModal.style.display = 'block';
    }

    window.closeCreateIndexWizard = function() {
        if (!wizardModal) return;
        wizardModal.style.display = 'none';
    }

    window.wizardNavigate = function(step) {
        if (!wizardSteps) return;
        if (step === 2 && !selectedProcessedJobData) {
            alert("Please select a processed dataset first.");
            return;
        }
        if (step === 3 && wizardAckValidationCheckbox && !wizardAckValidationCheckbox.checked) {
            alert("Please acknowledge the validation summary to proceed.");
            return;
        }

        currentWizardStep = step;
        wizardSteps.forEach(s => s.style.display = 'none');
        const targetStep = document.getElementById(`wizard-step-${step}`);
        if (targetStep) {
            targetStep.style.display = 'block';
            if (step === 2) displayValidationSummaryForWizard();
            if (step === 4) populateWizardSummary();
        }
    }

    if(wizardAckValidationCheckbox && wizardStep2NextButton) {
        wizardAckValidationCheckbox.addEventListener('change', function() {
            wizardStep2NextButton.disabled = !this.checked;
        });
    }

    function populateWizardSummary() {
        if (!selectedProcessedJobData || !indexNameInput || !indexTypeSelect || !wizardAttributesSelect) return;
        if(summaryDataPath) summaryDataPath.textContent = selectedProcessedJobData.location_of_promoted_data;
        if(summaryIndexName) summaryIndexName.textContent = indexNameInput.value;
        if(summaryIndexType) summaryIndexType.textContent = indexTypeSelect.value;

        const params = {};
        if (indexTypeSelect.value === "RTree" && rtreeMaxEntriesInput) params.max_entries = rtreeMaxEntriesInput.value;
        // Add other index type params here
        if(summaryIndexParams) summaryIndexParams.textContent = JSON.stringify(params);

        const selectedAttrs = Array.from(wizardAttributesSelect.selectedOptions).map(opt => opt.value);
        if(summaryIndexAttributes) summaryIndexAttributes.textContent = selectedAttrs.join(', ');
    }

    window.submitCreateIndex = async function() {
        if (!currentUser.token) { alert("Authentication error. Please login again."); return; }
        if (!selectedProcessedJobData || !indexNameInput.value || !indexTypeSelect.value) {
            alert("Missing required information for index creation.");
            return;
        }

        const indexParams = {};
        if (indexTypeSelect.value === "RTree" && rtreeMaxEntriesInput) {
            if (rtreeMaxEntriesInput.value) indexParams.max_entries = parseInt(rtreeMaxEntriesInput.value, 10);
        } else if (indexTypeSelect.value === "Elasticsearch") {
            if (esShardsInput && esShardsInput.value) indexParams.number_of_shards = parseInt(esShardsInput.value, 10);
            if (esReplicasInput && esReplicasInput.value) indexParams.number_of_replicas = parseInt(esReplicasInput.value, 10);
            // Add other Elasticsearch specific params here
        }
        // Add more 'else if' blocks for other index types

        const requestBody = {
            promoted_data_path: selectedProcessedJobData.location_of_promoted_data,
            index_name: indexNameInput.value,
            index_type: indexTypeSelect.value,
            index_parameters: indexParams,
            // attributes_to_index: Array.from(wizardAttributesSelect.selectedOptions).map(opt => opt.value) // If backend supports this
        };

        try {
            const response = await fetch('/indexes', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${currentUser.token}`
                },
                body: JSON.stringify(requestBody)
            });
            const responseData = await response.json();
            if (!response.ok) {
                throw new Error(responseData.detail || `Failed to create index: ${response.statusText}`);
            }
            alert(`Index creation initiated successfully! Index ID: ${responseData.index_id}, Status: ${responseData.status}`);
            closeCreateIndexWizard();
            loadIndexesTable(); // Refresh the table
        } catch (error) {
            console.error("Error creating index:", error);
            alert(`Error creating index: ${error.message}`);
        }
    }

    async function loadIndexesTable() {
        if (!indexesTableBody || !currentUser.token) return;
        const skeletonRowHtml = `<tr><td colspan="7"><div class="skeleton skeleton-text long"></div></td></tr>`;
        showLoading(indexesTableBody, 'skeleton', 5, skeletonRowHtml);
        try {
            const response = await fetch('/indexes', {
                headers: { 'Authorization': `Bearer ${currentUser.token}` }
            });
            if (!response.ok) throw new Error(`Failed to fetch indexes: ${response.statusText}`);
            const indexes = await response.json();
            indexesTableBody.innerHTML = ''; // Clear loading/skeleton
            if (indexes.length === 0) {
                indexesTableBody.innerHTML = '<tr><td colspan="7">No indexes found.</td></tr>';
                return;
            }
            indexes.forEach(idx => addIndexToTable(idx));
        } catch (error) {
            console.error("Error loading indexes table:", error);
            indexesTableBody.innerHTML = `<tr><td colspan="7">Error loading indexes: ${error.message}</td></tr>`;
        } finally {
            // hideLoading not strictly needed here as innerHTML is overwritten
        }
    }

    function addIndexToTable(idx) {
        if (!indexesTableBody) return;
        const row = indexesTableBody.insertRow();
        row.dataset.indexId = idx.index_id; // Store ID for easy access

        row.insertCell().textContent = idx.index_id;

        const nameCell = row.insertCell();
        const nameLink = document.createElement('a');
        nameLink.href = '#';
        nameLink.textContent = idx.index_name;
        nameLink.style.cursor = 'pointer';
        nameLink.onclick = (e) => {
            e.preventDefault();
            toggleIndexDetails(idx.index_id, row);
        };
        nameCell.appendChild(nameLink);

        row.insertCell().textContent = idx.index_type;
        row.insertCell().textContent = idx.status;
        row.insertCell().textContent = idx.version;
        row.insertCell().textContent = new Date(idx.created_at).toLocaleString();

        const actionsCell = row.insertCell();
        // Removed explicit "Details" button, using clickable name instead.
        // Add other actions like Query, Delete etc.
        const deleteBtn = document.createElement('button');
        deleteBtn.textContent = 'Delete';
        deleteBtn.style.backgroundColor = '#f44336'; // Red color for delete
        deleteBtn.style.color = 'white';
        deleteBtn.onclick = () => deleteIndex(idx.index_id, idx.index_name);
        actionsCell.appendChild(deleteBtn);
    }

    window.toggleIndexDetails = function(indexId, rowElement) {
        if (!indexDetailsArea) return;

        // Deselect other rows
        const allRows = indexesTableBody.querySelectorAll('tr');
        allRows.forEach(r => r.classList.remove('active-row-highlight')); // Define .active-row-highlight in CSS

        if (currentlyDisplayedIndexId === indexId && indexDetailsArea.style.display === 'block') {
            indexDetailsArea.style.display = 'none';
            currentlyDisplayedIndexId = null;
        } else {
            viewIndexDetails(indexId); // Fetches and displays
            rowElement.classList.add('active-row-highlight');
            currentlyDisplayedIndexId = indexId;
        }
    }

    async function viewIndexDetails(indexId) { // Renamed and modified from window.viewIndexDetails
        if (!indexDetailsArea || !indexDetailsContent || !currentUser.token) return;

        indexDetailsArea.style.display = 'block'; // Show area first
        const detailsSpan = indexDetailsArea.querySelector('h4 span');
        if(detailsSpan) detailsSpan.textContent = indexId; // Update which index is being shown

        showLoading(indexDetailsContent, 'spinner');

        try {
            const response = await fetch(`/indexes/${indexId}`, {
                 headers: { 'Authorization': `Bearer ${currentUser.token}` }
            });
            hideLoading(indexDetailsContent); // Hide spinner after fetch attempt
            if (!response.ok) throw new Error(`Failed to fetch index details: ${response.statusText}`);
            const details = await response.json();
            indexDetailsContent.textContent = JSON.stringify(details, null, 2);
        } catch (error) {
            console.error("Error fetching index details:", error);
            indexDetailsContent.textContent = `Error: ${error.message}`;
        }
    }

    window.deleteIndex = async function(indexId, indexName) {
        if (!currentUser.token) { alert("Authentication error."); return; }
        if (confirm(`Are you sure you want to delete index "${indexName}" (ID: ${indexId})? This action only removes metadata.`)) {
            try {
                const response = await fetch(`/indexes/${indexId}`, {
                    method: 'DELETE',
                    headers: { 'Authorization': `Bearer ${currentUser.token}` }
                });
                const responseData = await response.json();
                if (!response.ok) {
                    throw new Error(responseData.detail || `Failed to delete index: ${response.statusText}`);
                }
                alert(`Index ${indexName} deleted successfully (metadata only).`);
                loadIndexesTable(); // Refresh
                if (indexDetailsArea && indexDetailsArea.style.display === 'block' && indexDetailsContent.textContent.includes(indexId)) {
                    indexDetailsArea.style.display = 'none'; // Hide details if they were for this job
                }
            } catch (error) {
                console.error("Error deleting index:", error);
                alert(`Error deleting index: ${error.message}`);
            }
        }
    }

    if (showCreateIndexWizardButton) {
        showCreateIndexWizardButton.addEventListener('click', openCreateIndexWizard);
    }

    // Overload showView to load table when index management view is shown
    const originalShowView = showView;
    window.showView = function(viewId) { // Make it global for nav links or change nav links to use event listeners
        originalShowView(viewId);
        if (viewId === 'index-management-view' && currentUser.token) {
            loadIndexesTable();
            if(indexDetailsArea) indexDetailsArea.style.display = 'none'; // Hide details when switching to view
        }
         if (viewId === 'ingestion-view' && currentUser.token) {
            // Potentially load/refresh ingestion/quarantine tables too
            if (quarantineJobsTableBody && quarantineJobsTableBody.rows.length === 0) { // Simple check to only load once if empty
                 addJobToQuarantineTable({id: 'job_q1', filename: 'error_file.shp', quarantined_at: new Date(2023, 9, 25).toLocaleDateString(), error_summary: 'Invalid geometry detected after CRS transform.'});
                 addJobToQuarantineTable({id: 'job_q2', filename: 'corrupt_archive.zip', quarantined_at: new Date(2023, 9, 26).toLocaleDateString(), error_summary: 'Security scan: Potential zip bomb.'});
            }
        }
    };
    // Ensure initial active view also triggers its load logic
     if (initialActiveView && initialActiveView.id === 'index-management-view' && currentUser.token) {
        loadIndexesTable();
    }
     if (initialActiveView && initialActiveView.id === 'ingestion-view' && currentUser.token) {
         if (quarantineJobsTableBody && quarantineJobsTableBody.rows.length === 0) {
            addJobToQuarantineTable({id: 'job_q1', filename: 'error_file.shp', quarantined_at: new Date(2023, 9, 25).toLocaleDateString(), error_summary: 'Invalid geometry detected after CRS transform.'});
            addJobToQuarantineTable({id: 'job_q2', filename: 'corrupt_archive.zip', quarantined_at: new Date(2023, 9, 26).toLocaleDateString(), error_summary: 'Security scan: Potential zip bomb.'});
         }
    }

    // --- Query Interface Logic ---
    const queryIndexSelect = document.getElementById('query-index-select');
    const queryTypeSelect = document.getElementById('query-type-select');
    const queryParamsInputs = document.getElementById('query-params-inputs');
    const executeQueryButton = document.getElementById('execute-query-button');
    const queryResultsSummary = document.getElementById('query-results-summary');
    const queryResultsJson = document.getElementById('query-results-json');
    const queryHistoryList = document.getElementById('query-history-list');

    async function populateQueryIndexSelect() {
        if (!queryIndexSelect || !currentUser.token) return;

        const currentSelection = queryIndexSelect.value;
        showLoading(queryIndexSelect.parentNode, 'spinner');
        queryIndexSelect.innerHTML = '';
        const placeholder = document.createElement('option');
        placeholder.value = "";
        placeholder.textContent = "Loading available indexes...";
        queryIndexSelect.appendChild(placeholder);

        try {
            const response = await fetch('/indexes?status=ready', {
                headers: { 'Authorization': `Bearer ${currentUser.token}` }
            });
            if (!response.ok) throw new Error(`Failed to fetch indexes: ${response.statusText}`);
            const indexes = await response.json();

            queryIndexSelect.innerHTML = ''; // Clear loading
            const defaultOption = document.createElement('option');
            defaultOption.value = "";
            defaultOption.textContent = "-- Select an Index --";
            queryIndexSelect.appendChild(defaultOption);

            const readyIndexes = indexes.filter(idx => idx.status === 'ready');

            if (readyIndexes.length === 0) {
                 const noDataOption = document.createElement('option');
                 noDataOption.value = "";
                 noDataOption.disabled = true;
                 noDataOption.textContent = "No ready indexes available.";
                queryIndexSelect.appendChild(noDataOption);
                hideLoading(queryIndexSelect.parentNode);
                return;
            }
            readyIndexes.forEach(idx => {
                const option = document.createElement('option');
                option.value = idx.index_id;
                option.textContent = `${idx.index_name} (ID: ${idx.index_id}, Type: ${idx.index_type})`;
                queryIndexSelect.appendChild(option);
            });
        } catch (error) {
            console.error("Error populating query index select:", error);
            queryIndexSelect.innerHTML = '<option value="">Error loading indexes.</option>';
        } finally {
            hideLoading(queryIndexSelect.parentNode);
            if (currentSelection) queryIndexSelect.value = currentSelection; // Preserve selection if possible
        }
    }

    function updateQueryParamsUI(queryType) {
        if (!queryParamsInputs) return;
        queryParamsInputs.innerHTML = ''; // Clear previous inputs

        if (queryType === 'bbox') {
            queryParamsInputs.innerHTML = `
                <label for="query-bbox-minx">Min X:</label><input type="number" id="query-bbox-minx" placeholder="min_x" step="any">
                <label for="query-bbox-miny">Min Y:</label><input type="number" id="query-bbox-miny" placeholder="min_y" step="any">
                <label for="query-bbox-maxx">Max X:</label><input type="number" id="query-bbox-maxx" placeholder="max_x" step="any">
                <label for="query-bbox-maxy">Max Y:</label><input type="number" id="query-bbox-maxy" placeholder="max_y" step="any">
            `;
        } else if (queryType === 'point_radius') {
            queryParamsInputs.innerHTML = `
                <label for="query-point-x">Point X:</label><input type="number" id="query-point-x" placeholder="point_x" step="any">
                <label for="query-point-y">Point Y:</label><input type="number" id="query-point-y" placeholder="point_y" step="any">
                <label for="query-radius">Radius:</label><input type="number" id="query-radius" placeholder="radius" step="any" min="0">
            `;
        }
        // Add more cases for other query types like KNN
    }

    if (queryTypeSelect) {
        queryTypeSelect.addEventListener('change', (event) => {
            updateQueryParamsUI(event.target.value);
        });
        // Initialize with default query type
        updateQueryParamsUI(queryTypeSelect.value);
    }

    async function executeQuery() {
        if (!currentUser.token) { alert("Please login to execute queries."); return; }
        if (!queryIndexSelect || !queryTypeSelect || !queryParamsInputs || !queryResultsSummary || !queryResultsJson) return;

        const indexId = queryIndexSelect.value;
        const queryType = queryTypeSelect.value;

        if (!indexId) {
            alert("Please select an index.");
            return;
        }

        const parameters = {};
        if (queryType === 'bbox') {
            parameters.bbox = [
                parseFloat(document.getElementById('query-bbox-minx')?.value),
                parseFloat(document.getElementById('query-bbox-miny')?.value),
                parseFloat(document.getElementById('query-bbox-maxx')?.value),
                parseFloat(document.getElementById('query-bbox-maxy')?.value)
            ];
            if (parameters.bbox.some(isNaN)) { alert("Invalid BBox coordinates."); return; }
        } else if (queryType === 'point_radius') {
            parameters.point = [
                parseFloat(document.getElementById('query-point-x')?.value),
                parseFloat(document.getElementById('query-point-y')?.value)
            ];
            parameters.radius = parseFloat(document.getElementById('query-radius')?.value);
            if (parameters.point.some(isNaN) || isNaN(parameters.radius)) { alert("Invalid point or radius values."); return; }
        }
        // Add parameter collection for other query types

        showLoading(queryResultsJson.parentNode, 'spinner'); // Show spinner in results area
        queryResultsSummary.textContent = 'Executing query...';
        queryResultsJson.textContent = '';

        try {
            const response = await fetch('/query/execute', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${currentUser.token}`
                },
                body: JSON.stringify({ index_id: indexId, query_type: queryType, parameters: parameters })
            });
            hideLoading(queryResultsJson.parentNode); // Hide spinner after fetch
            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.detail || `Query failed: ${response.statusText}`);
            }
            queryResultsSummary.textContent = `Found ${data.count} results in ${data.query_time_ms}ms. ${data.message || ''}`;
            queryResultsJson.textContent = JSON.stringify(data.results, null, 2);
            loadQueryHistory(); // Refresh history
        } catch (error) {
            console.error("Error executing query:", error);
            queryResultsSummary.textContent = `Error: ${error.message}`;
            queryResultsJson.textContent = ''; // Clear any partial results/loading text
            hideLoading(queryResultsJson.parentNode); // Ensure spinner is hidden on error too
        }
    }

    if (executeQueryButton) {
        executeQueryButton.addEventListener('click', executeQuery);
    }

    async function loadQueryHistory() {
        if (!queryHistoryList || !currentUser.token) return;
        showLoading(queryHistoryList, 'skeleton', 3, '<li><div class="skeleton skeleton-text medium"></div></li>');
        try {
            const response = await fetch('/query/history', {
                headers: { 'Authorization': `Bearer ${currentUser.token}` }
            });
            if (!response.ok) throw new Error(`Failed to fetch query history: ${response.statusText}`);
            const historyItems = await response.json();
            queryHistoryList.innerHTML = ''; // Clear loading/skeleton
            if (historyItems.length === 0) {
                queryHistoryList.innerHTML = '<li>No query history found.</li>';
                return;
            }
            historyItems.forEach(item => {
                const li = document.createElement('li');
                li.textContent = `${new Date(item.timestamp).toLocaleString()}: ${item.request_data.query_type} on ${item.request_data.index_id} (Params: ${JSON.stringify(item.request_data.parameters)})`;
                li.title = "Click to re-populate form (not fully implemented)";
                li.onclick = () => { // Basic re-population
                    queryIndexSelect.value = item.request_data.index_id;
                    queryTypeSelect.value = item.request_data.query_type;
                    updateQueryParamsUI(item.request_data.query_type); // This creates the elements

                    const params = item.request_data.parameters;
                    if (item.request_data.query_type === 'bbox' && params.bbox && params.bbox.length === 4) {
                        const minXInput = document.getElementById('query-bbox-minx');
                        const minYInput = document.getElementById('query-bbox-miny');
                        const maxXInput = document.getElementById('query-bbox-maxx');
                        const maxYInput = document.getElementById('query-bbox-maxy');
                        if (minXInput) minXInput.value = params.bbox[0];
                        if (minYInput) minYInput.value = params.bbox[1];
                        if (maxXInput) maxXInput.value = params.bbox[2];
                        if (maxYInput) maxYInput.value = params.bbox[3];
                    } else if (item.request_data.query_type === 'point_radius' && params.point && params.point.length === 2 && params.radius !== undefined) {
                        const pointXInput = document.getElementById('query-point-x');
                        const pointYInput = document.getElementById('query-point-y');
                        const radiusInput = document.getElementById('query-radius');
                        if (pointXInput) pointXInput.value = params.point[0];
                        if (pointYInput) pointYInput.value = params.point[1];
                        if (radiusInput) radiusInput.value = params.radius;
                    }
                    // Add more cases for other query types as they are implemented

                    alert("Query form populated from history.");
                };
                queryHistoryList.appendChild(li);
            });
        } catch (error) {
            console.error("Error loading query history:", error);
            queryHistoryList.innerHTML = `<li>Error loading history: ${error.message}</li>`;
        }
    }

    // Extend showView to load query interface data
    const originalShowViewForQuery = window.showView; // Assuming showView is now global
    window.showView = function(viewId) {
        originalShowViewForQuery(viewId); // Call the original/extended showView
        if (viewId === 'query-interface-view' && currentUser.token) {
            populateQueryIndexSelect();
            loadQueryHistory();
            updateQueryParamsUI(queryTypeSelect ? queryTypeSelect.value : 'bbox'); // Ensure params UI matches default
        }
    };
    // Initial load if query view is active by default (and user logged in)
    if (initialActiveView && initialActiveView.id === 'query-interface-view' && currentUser.token) {
        populateQueryIndexSelect();
        loadQueryHistory();
        updateQueryParamsUI(queryTypeSelect ? queryTypeSelect.value : 'bbox');
    }

});
