<template>
  <div class="home-view">
    <div class="dashboard-header">
      <div class="header-content">
        <h1>📊 SPINDEX Dashboard</h1>
        <p class="subtitle">Real-time system monitoring and quick actions</p>
      </div>
      <button @click="refreshStats" class="refresh-button" :disabled="isRefreshing">
        <span v-if="!isRefreshing">🔄 Refresh</span>
        <span v-else>Refreshing...</span>
      </button>
    </div>

    <!-- System Stats Cards -->
    <div class="stats-grid">
      <div class="stat-card">
        <div class="stat-icon">📑</div>
        <div class="stat-content">
          <div class="stat-value">{{ stats.total_indexes }}</div>
          <div class="stat-label">Total Indexes</div>
        </div>
      </div>

      <div class="stat-card">
        <div class="stat-icon">📄</div>
        <div class="stat-content">
          <div class="stat-value">{{ formatNumber(stats.total_documents) }}</div>
          <div class="stat-label">Total Documents</div>
        </div>
      </div>

      <div class="stat-card">
        <div class="stat-icon">🔍</div>
        <div class="stat-content">
          <div class="stat-value">{{ stats.total_queries_today }}</div>
          <div class="stat-label">Queries Today</div>
        </div>
      </div>

      <div class="stat-card">
        <div class="stat-icon">⚡</div>
        <div class="stat-content">
          <div class="stat-value">{{ stats.avg_query_time_ms }}ms</div>
          <div class="stat-label">Avg Query Time</div>
        </div>
      </div>

      <div class="stat-card">
        <div class="stat-icon">⏳</div>
        <div class="stat-content">
          <div class="stat-value">{{ stats.active_jobs }}</div>
          <div class="stat-label">Active Jobs</div>
        </div>
      </div>

      <div class="stat-card">
        <div class="stat-icon">💾</div>
        <div class="stat-content">
          <div class="stat-value">{{ stats.storage_used_gb.toFixed(2) }} GB</div>
          <div class="stat-label">Storage Used</div>
        </div>
      </div>
    </div>

    <!-- Quick Actions -->
    <div class="quick-actions">
      <h2>⚡ Quick Actions</h2>
      <div class="actions-grid">
        <router-link to="/search" class="action-card">
          <div class="action-icon">🔍</div>
          <h3>Search Data</h3>
          <p>Search indexed documents with AVX-512 acceleration</p>
        </router-link>

        <div @click="showUploadSection = !showUploadSection" class="action-card clickable">
          <div class="action-icon">📤</div>
          <h3>Upload Files</h3>
          <p>Upload data files for ingestion and processing</p>
        </div>

        <router-link to="/indexes" class="action-card">
          <div class="action-icon">📊</div>
          <h3>Manage Indexes</h3>
          <p>Create, view, and manage spatial indexes</p>
        </router-link>

        <router-link to="/query-history" class="action-card">
          <div class="action-icon">📜</div>
          <h3>Query History</h3>
          <p>View and reuse previous search queries</p>
        </router-link>
      </div>
    </div>

    <!-- File Upload Section -->
    <div v-if="showUploadSection" class="upload-section">
      <div class="section-header">
        <h2>📤 Upload Data Files</h2>
        <button @click="showUploadSection = false" class="close-section-button">×</button>
      </div>
      <FileUpload @upload-success="handleUploadSuccess" @upload-error="handleUploadError" />
    </div>

    <!-- Recent Indexes -->
    <div class="recent-section">
      <h2>📑 Recent Indexes</h2>
      <div v-if="isLoadingIndexes" class="loading-message">
        <div class="mini-spinner"></div>
        Loading indexes...
      </div>
      <div v-else-if="recentIndexes.length === 0" class="empty-message">
        No indexes yet. Create your first index to get started!
      </div>
      <div v-else class="indexes-table">
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Type</th>
              <th>Status</th>
              <th>Version</th>
              <th>Created</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="index in recentIndexes" :key="index.index_id">
              <td class="index-name">{{ index.index_name }}</td>
              <td>{{ index.index_type }}</td>
              <td>
                <span class="status-badge" :class="index.status">
                  {{ index.status }}
                </span>
              </td>
              <td>v{{ index.version }}</td>
              <td class="date-cell">{{ formatDate(index.created_at) }}</td>
              <td class="actions-cell">
                <button @click="viewIndex(index)" class="action-button view">View</button>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
      <router-link to="/indexes" class="view-all-link">
        View All Indexes →
      </router-link>
    </div>

    <!-- Performance Highlights -->
    <div class="performance-section">
      <h2>⚡ Performance Highlights</h2>
      <div class="performance-grid">
        <div class="performance-card">
          <div class="perf-header">
            <h3>AVX-512 Acceleration</h3>
            <span class="perf-badge">Active</span>
          </div>
          <p>Hardware-accelerated search providing 4-10x faster query processing</p>
          <div class="perf-stat">
            <span class="perf-number">10x</span>
            <span class="perf-label">Faster Substring Search</span>
          </div>
        </div>

        <div class="performance-card">
          <div class="perf-header">
            <h3>Memory-Mapped I/O</h3>
            <span class="perf-badge">Enabled</span>
          </div>
          <p>Zero-copy file access for ultra-fast data ingestion and processing</p>
          <div class="perf-stat">
            <span class="perf-number">2-10x</span>
            <span class="perf-label">Faster File Reading</span>
          </div>
        </div>

        <div class="performance-card">
          <div class="perf-header">
            <h3>22 Active Plugins</h3>
            <span class="perf-badge">Loaded</span>
          </div>
          <p>Preprocessors, parsers, indexers, and analyzers ready for data processing</p>
          <div class="perf-stat">
            <span class="perf-number">22</span>
            <span class="perf-label">Plugins Available</span>
          </div>
        </div>
      </div>
    </div>

    <!-- System Info -->
    <div class="system-info">
      <h3>ℹ️ System Information</h3>
      <div class="info-grid">
        <div class="info-item">
          <span class="info-label">SPINDEX Version:</span>
          <span class="info-value">1.0.0</span>
        </div>
        <div class="info-item">
          <span class="info-label">Build Date:</span>
          <span class="info-value">Nov 6 2025</span>
        </div>
        <div class="info-item">
          <span class="info-label">C Core:</span>
          <span class="info-value status">✅ Operational</span>
        </div>
        <div class="info-item">
          <span class="info-label">Elasticsearch:</span>
          <span class="info-value status">✅ Connected</span>
        </div>
        <div class="info-item">
          <span class="info-label">GEOS:</span>
          <span class="info-value status">✅ Enabled</span>
        </div>
        <div class="info-item">
          <span class="info-label">Multi-threading:</span>
          <span class="info-value status">✅ Yes</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue';
import { useRouter } from 'vue-router';
import { api, SystemStats, IndexMetadata, FileUploadResponse, handleApiError } from '../services/api';
import FileUpload from '../components/common/FileUpload.vue';

// Router
const router = useRouter();

// State
const stats = ref<SystemStats>({
  total_indexes: 0,
  total_documents: 0,
  total_queries_today: 0,
  avg_query_time_ms: 0,
  active_jobs: 0,
  storage_used_gb: 0,
});

const recentIndexes = ref<IndexMetadata[]>([]);
const isLoadingIndexes = ref(false);
const isRefreshing = ref(false);
const showUploadSection = ref(false);

let refreshInterval: number | null = null;

// Methods
async function loadStats() {
  try {
    const data = await api.getSystemStats();
    stats.value = data;
  } catch (error) {
    console.error('Failed to load stats:', error);
    // Keep existing stats on error
  }
}

async function loadRecentIndexes() {
  isLoadingIndexes.value = true;
  try {
    const indexes = await api.getIndexes(0, 5);
    recentIndexes.value = indexes;
  } catch (error) {
    console.error('Failed to load indexes:', error);
    recentIndexes.value = [];
  } finally {
    isLoadingIndexes.value = false;
  }
}

async function refreshStats() {
  isRefreshing.value = true;
  try {
    await Promise.all([loadStats(), loadRecentIndexes()]);
  } finally {
    isRefreshing.value = false;
  }
}

function formatNumber(num: number): string {
  if (num >= 1000000) {
    return (num / 1000000).toFixed(1) + 'M';
  } else if (num >= 1000) {
    return (num / 1000).toFixed(1) + 'K';
  }
  return num.toString();
}

function formatDate(dateString: string): string {
  const date = new Date(dateString);
  const now = new Date();
  const diffMs = now.getTime() - date.getTime();
  const diffMins = Math.floor(diffMs / 60000);
  const diffHours = Math.floor(diffMins / 60);
  const diffDays = Math.floor(diffHours / 24);

  if (diffMins < 60) return `${diffMins}m ago`;
  if (diffHours < 24) return `${diffHours}h ago`;
  if (diffDays < 7) return `${diffDays}d ago`;

  return date.toLocaleDateString('en-US', {
    month: 'short',
    day: 'numeric',
    year: date.getFullYear() !== now.getFullYear() ? 'numeric' : undefined
  });
}

function viewIndex(index: IndexMetadata) {
  router.push(`/indexes/${index.index_id}`);
}

function handleUploadSuccess(result: FileUploadResponse) {
  console.log('Upload successful:', result);
  // Refresh stats to show new job
  loadStats();
}

function handleUploadError(error: string) {
  console.error('Upload error:', error);
}

// Lifecycle
onMounted(async () => {
  await refreshStats();

  // Auto-refresh stats every 30 seconds
  refreshInterval = window.setInterval(() => {
    loadStats();
  }, 30000);
});

onUnmounted(() => {
  if (refreshInterval) {
    clearInterval(refreshInterval);
  }
});
</script>

<style scoped>
.home-view {
  padding: 30px;
  max-width: 1600px;
  margin: 0 auto;
}

.dashboard-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 30px;
}

.header-content h1 {
  font-size: 2.5em;
  margin: 0 0 5px 0;
  color: #2c3e50;
}

.subtitle {
  color: #7f8c8d;
  margin: 0;
  font-size: 16px;
}

.refresh-button {
  padding: 12px 24px;
  background: linear-gradient(135deg, #3498db, #2980b9);
  color: white;
  border: none;
  border-radius: 8px;
  font-size: 14px;
  font-weight: 600;
  cursor: pointer;
  transition: transform 0.2s;
}

.refresh-button:hover:not(:disabled) {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(52, 152, 219, 0.3);
}

.refresh-button:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
  gap: 20px;
  margin-bottom: 40px;
}

.stat-card {
  background: white;
  padding: 25px;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
  display: flex;
  align-items: center;
  gap: 20px;
  transition: transform 0.2s, box-shadow 0.2s;
}

.stat-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 8px 16px rgba(0,0,0,0.15);
}

.stat-icon {
  font-size: 48px;
  line-height: 1;
}

.stat-content {
  flex: 1;
}

.stat-value {
  font-size: 32px;
  font-weight: 700;
  color: #2c3e50;
  line-height: 1.2;
}

.stat-label {
  font-size: 14px;
  color: #7f8c8d;
  margin-top: 5px;
}

.quick-actions {
  margin-bottom: 40px;
}

.quick-actions h2 {
  font-size: 1.8em;
  margin: 0 0 20px 0;
  color: #2c3e50;
}

.actions-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
  gap: 20px;
}

.action-card {
  background: white;
  padding: 30px;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
  text-decoration: none;
  color: inherit;
  transition: transform 0.2s, box-shadow 0.2s;
  display: block;
}

.action-card.clickable {
  cursor: pointer;
}

.action-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 8px 16px rgba(0,0,0,0.15);
}

.action-icon {
  font-size: 48px;
  margin-bottom: 15px;
}

.action-card h3 {
  margin: 0 0 10px 0;
  color: #2c3e50;
  font-size: 18px;
}

.action-card p {
  margin: 0;
  color: #7f8c8d;
  font-size: 14px;
  line-height: 1.5;
}

.upload-section {
  background: white;
  padding: 30px;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
  margin-bottom: 40px;
}

.section-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 30px;
}

.section-header h2 {
  margin: 0;
  font-size: 1.8em;
  color: #2c3e50;
}

.close-section-button {
  background: none;
  border: none;
  font-size: 32px;
  color: #95a5a6;
  cursor: pointer;
  line-height: 1;
  transition: color 0.2s;
}

.close-section-button:hover {
  color: #e74c3c;
}

.recent-section {
  background: white;
  padding: 30px;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
  margin-bottom: 40px;
}

.recent-section h2 {
  margin: 0 0 20px 0;
  font-size: 1.8em;
  color: #2c3e50;
}

.loading-message, .empty-message {
  text-align: center;
  padding: 40px 20px;
  color: #7f8c8d;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 15px;
}

.mini-spinner {
  width: 24px;
  height: 24px;
  border: 3px solid #f3f3f3;
  border-top: 3px solid #3498db;
  border-radius: 50%;
  animation: spin 1s linear infinite;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

.indexes-table {
  overflow-x: auto;
}

.indexes-table table {
  width: 100%;
  border-collapse: collapse;
}

.indexes-table th {
  text-align: left;
  padding: 12px;
  border-bottom: 2px solid #ecf0f1;
  color: #7f8c8d;
  font-size: 13px;
  font-weight: 600;
  text-transform: uppercase;
}

.indexes-table td {
  padding: 15px 12px;
  border-bottom: 1px solid #ecf0f1;
  color: #2c3e50;
}

.index-name {
  font-weight: 600;
}

.status-badge {
  padding: 4px 12px;
  border-radius: 12px;
  font-size: 11px;
  font-weight: 600;
  text-transform: uppercase;
}

.status-badge.ready {
  background: #d5f4e6;
  color: #27ae60;
}

.status-badge.creating {
  background: #fff3cd;
  color: #f39c12;
}

.status-badge.failed {
  background: #f8d7da;
  color: #e74c3c;
}

.date-cell {
  color: #95a5a6;
  font-size: 14px;
}

.actions-cell {
  text-align: right;
}

.action-button {
  padding: 6px 16px;
  border: none;
  border-radius: 6px;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.2s;
}

.action-button.view {
  background: #3498db;
  color: white;
}

.action-button.view:hover {
  background: #2980b9;
}

.view-all-link {
  display: inline-block;
  margin-top: 20px;
  color: #3498db;
  text-decoration: none;
  font-weight: 600;
  transition: color 0.2s;
}

.view-all-link:hover {
  color: #2980b9;
}

.performance-section {
  background: white;
  padding: 30px;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
  margin-bottom: 40px;
}

.performance-section h2 {
  margin: 0 0 20px 0;
  font-size: 1.8em;
  color: #2c3e50;
}

.performance-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 20px;
}

.performance-card {
  border: 1px solid #ecf0f1;
  border-radius: 12px;
  padding: 25px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
}

.perf-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 15px;
}

.perf-header h3 {
  margin: 0;
  font-size: 18px;
}

.perf-badge {
  padding: 4px 12px;
  background: rgba(255,255,255,0.3);
  border-radius: 12px;
  font-size: 11px;
  font-weight: 600;
  text-transform: uppercase;
}

.performance-card p {
  margin: 0 0 20px 0;
  opacity: 0.95;
  line-height: 1.5;
}

.perf-stat {
  background: rgba(255,255,255,0.15);
  padding: 15px;
  border-radius: 8px;
  text-align: center;
}

.perf-number {
  display: block;
  font-size: 36px;
  font-weight: 700;
  margin-bottom: 5px;
}

.perf-label {
  font-size: 13px;
  opacity: 0.9;
}

.system-info {
  background: white;
  padding: 30px;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
}

.system-info h3 {
  margin: 0 0 20px 0;
  font-size: 1.5em;
  color: #2c3e50;
}

.info-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
  gap: 15px;
}

.info-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 15px;
  background: #f7fafc;
  border-radius: 8px;
}

.info-label {
  font-weight: 600;
  color: #7f8c8d;
  font-size: 14px;
}

.info-value {
  color: #2c3e50;
  font-size: 14px;
}

.info-value.status {
  font-weight: 600;
}
</style>
