<template>
  <div class="file-upload-container">
    <div
      class="drop-zone"
      :class="{ 'drag-over': isDragging, 'uploading': isUploading }"
      @drop.prevent="handleDrop"
      @dragover.prevent="isDragging = true"
      @dragleave.prevent="isDragging = false"
      @click="triggerFileSelect"
    >
      <input
        ref="fileInput"
        type="file"
        @change="handleFileSelect"
        accept=".json,.csv,.geojson,.txt,.pdf,.doc,.docx,.xls,.xlsx,.rtf,.sqlite,.db"
        style="display: none"
      />

      <div v-if="!isUploading && !uploadComplete" class="drop-zone-content">
        <div class="upload-icon">📁</div>
        <h3>{{ isDragging ? 'Drop file here' : 'Drag & Drop or Click to Upload' }}</h3>
        <p class="supported-formats">
          Supported: JSON, CSV, GeoJSON, TXT, PDF, DOC, XLS, RTF, SQLite
        </p>
        <button class="browse-button">Browse Files</button>
      </div>

      <div v-if="isUploading" class="uploading-content">
        <div class="progress-circle">
          <svg width="120" height="120">
            <circle
              cx="60"
              cy="60"
              r="50"
              fill="none"
              stroke="#e0e0e0"
              stroke-width="10"
            />
            <circle
              cx="60"
              cy="60"
              r="50"
              fill="none"
              stroke="#3498db"
              stroke-width="10"
              stroke-dasharray="314"
              :stroke-dashoffset="314 - (314 * uploadProgress) / 100"
              transform="rotate(-90 60 60)"
              style="transition: stroke-dashoffset 0.3s"
            />
          </svg>
          <div class="progress-text">{{ uploadProgress }}%</div>
        </div>
        <h3>Uploading {{ selectedFile?.name }}</h3>
        <p>{{ formatFileSize(selectedFile?.size || 0) }}</p>
      </div>

      <div v-if="uploadComplete" class="success-content">
        <div class="success-icon">✅</div>
        <h3>Upload Successful!</h3>
        <p class="job-info">
          <strong>Job ID:</strong> {{ uploadResult?.job_id }}
        </p>
        <p class="filename">{{ uploadResult?.filename }}</p>
        <button @click="resetUpload" class="upload-another-button">
          Upload Another File
        </button>
      </div>
    </div>

    <div v-if="error" class="error-message">
      <strong>Upload Failed:</strong> {{ error }}
      <button @click="error = ''; resetUpload()" class="close-button">×</button>
    </div>

    <div v-if="uploadResult && !error" class="upload-details">
      <h4>📄 File Details</h4>
      <div class="detail-row">
        <span class="label">Original Filename:</span>
        <span class="value">{{ uploadResult.filename }}</span>
      </div>
      <div class="detail-row">
        <span class="label">Job ID:</span>
        <span class="value code">{{ uploadResult.job_id }}</span>
      </div>
      <div class="detail-row">
        <span class="label">Staged Path:</span>
        <span class="value code">{{ uploadResult.staged_filepath }}</span>
      </div>
      <div class="detail-row">
        <span class="label">Status:</span>
        <span class="value">
          <span class="status-badge processing">Processing</span>
        </span>
      </div>
      <button @click="viewJobDetails" class="view-job-button">
        View Job Status
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import { api, FileUploadResponse, handleApiError } from '../../services/api';

// Props & Emits
const emit = defineEmits<{
  (e: 'upload-success', result: FileUploadResponse): void;
  (e: 'upload-error', error: string): void;
}>();

// State
const fileInput = ref<HTMLInputElement | null>(null);
const selectedFile = ref<File | null>(null);
const isDragging = ref(false);
const isUploading = ref(false);
const uploadProgress = ref(0);
const uploadComplete = ref(false);
const uploadResult = ref<FileUploadResponse | null>(null);
const error = ref('');

// Methods
function triggerFileSelect() {
  if (!isUploading.value) {
    fileInput.value?.click();
  }
}

function handleFileSelect(event: Event) {
  const target = event.target as HTMLInputElement;
  if (target.files && target.files.length > 0) {
    uploadFile(target.files[0]);
  }
}

function handleDrop(event: DragEvent) {
  isDragging.value = false;
  if (event.dataTransfer?.files && event.dataTransfer.files.length > 0) {
    uploadFile(event.dataTransfer.files[0]);
  }
}

async function uploadFile(file: File) {
  selectedFile.value = file;
  isUploading.value = true;
  uploadProgress.value = 0;
  error.value = '';
  uploadResult.value = null;

  try {
    const result = await api.uploadFile(file, (progress) => {
      uploadProgress.value = progress;
    });

    uploadResult.value = result;
    uploadComplete.value = true;
    emit('upload-success', result);
  } catch (err: any) {
    error.value = handleApiError(err);
    emit('upload-error', error.value);
    resetUpload();
  } finally {
    isUploading.value = false;
  }
}

function resetUpload() {
  selectedFile.value = null;
  isUploading.value = false;
  uploadProgress.value = 0;
  uploadComplete.value = false;
  if (fileInput.value) {
    fileInput.value.value = '';
  }
}

function formatFileSize(bytes: number): string {
  if (bytes === 0) return '0 Bytes';
  const k = 1024;
  const sizes = ['Bytes', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
}

function viewJobDetails() {
  if (uploadResult.value) {
    // TODO: Navigate to job details page or open modal
    alert(`Job ID: ${uploadResult.value.job_id}\n\nClick OK to view job status page.`);
  }
}
</script>

<style scoped>
.file-upload-container {
  max-width: 800px;
  margin: 0 auto;
}

.drop-zone {
  border: 3px dashed #cbd5e0;
  border-radius: 16px;
  padding: 60px 40px;
  text-align: center;
  background: #f7fafc;
  cursor: pointer;
  transition: all 0.3s ease;
  position: relative;
}

.drop-zone.drag-over {
  border-color: #3498db;
  background: #ebf8ff;
  transform: scale(1.02);
}

.drop-zone.uploading {
  cursor: default;
  border-color: #3498db;
  background: white;
}

.drop-zone-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 15px;
}

.upload-icon {
  font-size: 64px;
  margin-bottom: 10px;
  animation: bounce 2s infinite;
}

@keyframes bounce {
  0%, 100% { transform: translateY(0); }
  50% { transform: translateY(-10px); }
}

.drop-zone h3 {
  margin: 0;
  font-size: 24px;
  color: #2c3e50;
}

.supported-formats {
  color: #7f8c8d;
  font-size: 14px;
  margin: 0;
}

.browse-button {
  padding: 12px 30px;
  background: linear-gradient(135deg, #3498db, #2980b9);
  color: white;
  border: none;
  border-radius: 8px;
  font-size: 16px;
  font-weight: 600;
  cursor: pointer;
  transition: transform 0.2s;
  margin-top: 10px;
}

.browse-button:hover {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(52, 152, 219, 0.3);
}

.uploading-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 20px;
}

.progress-circle {
  position: relative;
  display: inline-block;
}

.progress-text {
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  font-size: 24px;
  font-weight: 700;
  color: #3498db;
}

.uploading-content h3 {
  margin: 0;
  color: #2c3e50;
}

.uploading-content p {
  margin: 0;
  color: #7f8c8d;
}

.success-content {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 15px;
}

.success-icon {
  font-size: 64px;
  animation: scaleIn 0.5s ease;
}

@keyframes scaleIn {
  from {
    transform: scale(0);
  }
  to {
    transform: scale(1);
  }
}

.success-content h3 {
  margin: 0;
  color: #27ae60;
  font-size: 24px;
}

.job-info {
  background: #f0f0f0;
  padding: 10px 20px;
  border-radius: 8px;
  font-family: monospace;
  font-size: 14px;
  margin: 0;
}

.filename {
  color: #7f8c8d;
  margin: 0;
}

.upload-another-button {
  padding: 12px 30px;
  background: #27ae60;
  color: white;
  border: none;
  border-radius: 8px;
  font-size: 16px;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.2s;
  margin-top: 10px;
}

.upload-another-button:hover {
  background: #229954;
}

.error-message {
  margin-top: 20px;
  background: #fee;
  border-left: 4px solid #e74c3c;
  padding: 15px 20px;
  border-radius: 6px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.close-button {
  background: none;
  border: none;
  font-size: 24px;
  cursor: pointer;
  color: #e74c3c;
}

.upload-details {
  margin-top: 30px;
  background: white;
  padding: 25px;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
}

.upload-details h4 {
  margin: 0 0 20px 0;
  color: #2c3e50;
  font-size: 18px;
}

.detail-row {
  display: flex;
  justify-content: space-between;
  padding: 12px 0;
  border-bottom: 1px solid #ecf0f1;
}

.detail-row:last-of-type {
  border-bottom: none;
}

.label {
  font-weight: 600;
  color: #7f8c8d;
}

.value {
  color: #2c3e50;
  text-align: right;
  max-width: 60%;
  word-break: break-all;
}

.value.code {
  font-family: monospace;
  font-size: 13px;
  background: #f7fafc;
  padding: 4px 8px;
  border-radius: 4px;
}

.status-badge {
  padding: 4px 12px;
  border-radius: 12px;
  font-size: 12px;
  font-weight: 600;
}

.status-badge.processing {
  background: #fff3cd;
  color: #f39c12;
}

.view-job-button {
  width: 100%;
  padding: 12px;
  background: #3498db;
  color: white;
  border: none;
  border-radius: 8px;
  font-size: 16px;
  font-weight: 600;
  cursor: pointer;
  margin-top: 20px;
  transition: background 0.2s;
}

.view-job-button:hover {
  background: #2980b9;
}
</style>
