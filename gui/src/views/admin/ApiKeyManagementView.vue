<template>
  <div class="api-key-management-view">
    <div class="header">
      <h2>🔑 API Key Management</h2>
      <p class="subtitle">Manage authentication tokens for SPINDEX API access</p>
    </div>

    <!-- Create New API Key -->
    <div class="create-key-section">
      <h3>Create New API Key</h3>
      <div class="form-group">
        <label for="key-name">Key Name</label>
        <input
          id="key-name"
          v-model="newKey.name"
          type="text"
          placeholder="e.g., Production API Key"
          class="text-input"
        />
      </div>

      <div class="form-group">
        <label for="key-role">Role</label>
        <select id="key-role" v-model="newKey.role" class="select-input">
          <option value="viewer">Viewer (Read-only)</option>
          <option value="analyst">Analyst (Read + Search)</option>
          <option value="admin">Admin (Full Access)</option>
        </select>
      </div>

      <div class="form-group">
        <label for="key-expiry">Expiration</label>
        <select id="key-expiry" v-model="newKey.expiryDays" class="select-input">
          <option :value="30">30 days</option>
          <option :value="90">90 days</option>
          <option :value="180">6 months</option>
          <option :value="365">1 year</option>
          <option :value="null">Never</option>
        </select>
      </div>

      <div class="form-group">
        <label>
          <input type="checkbox" v-model="newKey.rateLimitEnabled" />
          Enable Rate Limiting
        </label>
        <div v-if="newKey.rateLimitEnabled" class="rate-limit-options">
          <label>
            Requests per minute:
            <input
              v-model.number="newKey.rateLimit.perMinute"
              type="number"
              min="1"
              max="1000"
              class="number-input"
            />
          </label>
          <label>
            Requests per hour:
            <input
              v-model.number="newKey.rateLimit.perHour"
              type="number"
              min="1"
              max="10000"
              class="number-input"
            />
          </label>
        </div>
      </div>

      <button @click="createApiKey" :disabled="!canCreateKey" class="primary-button">
        Create API Key
      </button>
    </div>

    <!-- Success Message with Generated Key -->
    <div v-if="generatedKey" class="generated-key-alert">
      <h3>✅ API Key Created Successfully</h3>
      <p class="warning">⚠️ Save this key now! You won't be able to see it again.</p>
      <div class="key-display">
        <code>{{ generatedKey }}</code>
        <button @click="copyToClipboard(generatedKey)" class="copy-button">
          {{ copied ? '✓ Copied' : 'Copy' }}
        </button>
      </div>
      <button @click="generatedKey = null" class="secondary-button">Done</button>
    </div>

    <!-- Error Display -->
    <div v-if="error" class="error-message">
      <strong>Error:</strong> {{ error }}
      <button @click="error = ''" class="close-button">×</button>
    </div>

    <!-- API Keys List -->
    <div class="keys-list-section">
      <div class="section-header">
        <h3>Active API Keys</h3>
        <button @click="loadApiKeys" class="refresh-button" :disabled="loading">
          {{ loading ? '⟳ Loading...' : '↻ Refresh' }}
        </button>
      </div>

      <div v-if="loading" class="loading-state">
        <div class="spinner"></div>
        <p>Loading API keys...</p>
      </div>

      <div v-else-if="apiKeys.length === 0" class="empty-state">
        <p>No API keys found. Create one to get started.</p>
      </div>

      <div v-else class="keys-table">
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Role</th>
              <th>Key Preview</th>
              <th>Created</th>
              <th>Expires</th>
              <th>Last Used</th>
              <th>Status</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="key in apiKeys" :key="key.id" :class="{ expired: isExpired(key) }">
              <td>
                <strong>{{ key.name }}</strong>
              </td>
              <td>
                <span :class="'role-badge role-' + key.role">{{ key.role }}</span>
              </td>
              <td>
                <code class="key-preview">{{ key.keyPreview }}...</code>
              </td>
              <td>{{ formatDate(key.createdAt) }}</td>
              <td>
                <span v-if="key.expiresAt">{{ formatDate(key.expiresAt) }}</span>
                <span v-else class="never">Never</span>
              </td>
              <td>
                <span v-if="key.lastUsedAt">{{ formatDate(key.lastUsedAt) }}</span>
                <span v-else class="never">Never</span>
              </td>
              <td>
                <span v-if="isExpired(key)" class="status-badge status-expired">Expired</span>
                <span v-else-if="key.revoked" class="status-badge status-revoked">Revoked</span>
                <span v-else class="status-badge status-active">Active</span>
              </td>
              <td>
                <div class="action-buttons">
                  <button
                    v-if="!key.revoked && !isExpired(key)"
                    @click="revokeApiKey(key.id)"
                    class="danger-button small"
                  >
                    Revoke
                  </button>
                  <button @click="deleteApiKey(key.id)" class="danger-button small">
                    Delete
                  </button>
                </div>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <!-- Usage Statistics -->
    <div v-if="apiKeys.length > 0" class="statistics-section">
      <h3>Usage Statistics (Last 24 Hours)</h3>
      <div class="stats-grid">
        <div class="stat-card">
          <div class="stat-value">{{ totalRequests }}</div>
          <div class="stat-label">Total Requests</div>
        </div>
        <div class="stat-card">
          <div class="stat-value">{{ activeKeys }}</div>
          <div class="stat-label">Active Keys</div>
        </div>
        <div class="stat-card">
          <div class="stat-value">{{ expiringSoonCount }}</div>
          <div class="stat-label">Expiring Soon</div>
        </div>
        <div class="stat-card">
          <div class="stat-value">{{ revokedCount }}</div>
          <div class="stat-label">Revoked</div>
        </div>
      </div>
    </div>

    <!-- Documentation -->
    <div class="documentation-section">
      <h3>📖 API Key Usage</h3>
      <div class="doc-content">
        <p>Use your API key in requests by adding it to the <code>X-Auth-Token</code> header:</p>
        <pre><code>curl -H "X-Auth-Token: your-api-key-here" http://localhost:8000/api/endpoint</code></pre>

        <h4>Security Best Practices</h4>
        <ul>
          <li>Never commit API keys to version control</li>
          <li>Rotate keys regularly (every 90 days recommended)</li>
          <li>Use the least privileged role necessary</li>
          <li>Enable rate limiting for production keys</li>
          <li>Revoke unused keys immediately</li>
          <li>Monitor key usage in the statistics dashboard</li>
        </ul>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import axios from 'axios'

// Types
interface ApiKey {
  id: string
  name: string
  role: 'viewer' | 'analyst' | 'admin'
  keyPreview: string
  createdAt: string
  expiresAt: string | null
  lastUsedAt: string | null
  revoked: boolean
  rateLimitEnabled: boolean
  rateLimit: {
    perMinute: number
    perHour: number
  }
}

interface NewApiKeyForm {
  name: string
  role: 'viewer' | 'analyst' | 'admin'
  expiryDays: number | null
  rateLimitEnabled: boolean
  rateLimit: {
    perMinute: number
    perHour: number
  }
}

// State
const apiKeys = ref<ApiKey[]>([])
const loading = ref(false)
const error = ref('')
const generatedKey = ref<string | null>(null)
const copied = ref(false)

const newKey = ref<NewApiKeyForm>({
  name: '',
  role: 'viewer',
  expiryDays: 90,
  rateLimitEnabled: true,
  rateLimit: {
    perMinute: 60,
    perHour: 1000
  }
})

// Computed
const canCreateKey = computed(() => {
  return newKey.value.name.trim().length > 0
})

const totalRequests = computed(() => {
  return apiKeys.value.reduce((sum, key) => sum + (key as any).requestsLast24h || 0, 0)
})

const activeKeys = computed(() => {
  return apiKeys.value.filter(key => !key.revoked && !isExpired(key)).length
})

const expiringSoonCount = computed(() => {
  const now = new Date()
  const thirtyDaysFromNow = new Date(now.getTime() + 30 * 24 * 60 * 60 * 1000)
  return apiKeys.value.filter(key => {
    if (!key.expiresAt || key.revoked) return false
    const expiresAt = new Date(key.expiresAt)
    return expiresAt > now && expiresAt <= thirtyDaysFromNow
  }).length
})

const revokedCount = computed(() => {
  return apiKeys.value.filter(key => key.revoked).length
})

// Methods
const loadApiKeys = async () => {
  loading.value = true
  error.value = ''

  try {
    // Mock data for demonstration
    // In production, replace with: const response = await axios.get('/api/admin/api-keys')
    await new Promise(resolve => setTimeout(resolve, 500))

    // Mock data
    apiKeys.value = [
      {
        id: '1',
        name: 'Production API Key',
        role: 'admin',
        keyPreview: 'spx_prod_a1b2c3d4',
        createdAt: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000).toISOString(),
        expiresAt: new Date(Date.now() + 60 * 24 * 60 * 60 * 1000).toISOString(),
        lastUsedAt: new Date(Date.now() - 2 * 60 * 60 * 1000).toISOString(),
        revoked: false,
        rateLimitEnabled: true,
        rateLimit: { perMinute: 100, perHour: 5000 }
      },
      {
        id: '2',
        name: 'Development API Key',
        role: 'analyst',
        keyPreview: 'spx_dev_e5f6g7h8',
        createdAt: new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString(),
        expiresAt: null,
        lastUsedAt: new Date(Date.now() - 5 * 60 * 1000).toISOString(),
        revoked: false,
        rateLimitEnabled: true,
        rateLimit: { perMinute: 60, perHour: 1000 }
      }
    ]
  } catch (err: any) {
    error.value = err.response?.data?.detail || err.message || 'Failed to load API keys'
  } finally {
    loading.value = false
  }
}

const createApiKey = async () => {
  error.value = ''
  loading.value = true

  try {
    // Mock key generation
    // In production: const response = await axios.post('/api/admin/api-keys', newKey.value)
    await new Promise(resolve => setTimeout(resolve, 500))

    // Generate a mock key
    const prefix = `spx_${newKey.value.role}_`
    const randomPart = Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15)
    generatedKey.value = prefix + randomPart

    // Reset form
    newKey.value = {
      name: '',
      role: 'viewer',
      expiryDays: 90,
      rateLimitEnabled: true,
      rateLimit: {
        perMinute: 60,
        perHour: 1000
      }
    }

    // Reload keys
    await loadApiKeys()
  } catch (err: any) {
    error.value = err.response?.data?.detail || err.message || 'Failed to create API key'
  } finally {
    loading.value = false
  }
}

const revokeApiKey = async (id: string) => {
  if (!confirm('Are you sure you want to revoke this API key? This action cannot be undone.')) {
    return
  }

  try {
    // In production: await axios.post(`/api/admin/api-keys/${id}/revoke`)
    await new Promise(resolve => setTimeout(resolve, 300))

    const key = apiKeys.value.find(k => k.id === id)
    if (key) {
      key.revoked = true
    }
  } catch (err: any) {
    error.value = err.response?.data?.detail || err.message || 'Failed to revoke API key'
  }
}

const deleteApiKey = async (id: string) => {
  if (!confirm('Are you sure you want to delete this API key? This action cannot be undone.')) {
    return
  }

  try {
    // In production: await axios.delete(`/api/admin/api-keys/${id}`)
    await new Promise(resolve => setTimeout(resolve, 300))

    apiKeys.value = apiKeys.value.filter(k => k.id !== id)
  } catch (err: any) {
    error.value = err.response?.data?.detail || err.message || 'Failed to delete API key'
  }
}

const copyToClipboard = async (text: string) => {
  try {
    await navigator.clipboard.writeText(text)
    copied.value = true
    setTimeout(() => {
      copied.value = false
    }, 2000)
  } catch (err) {
    error.value = 'Failed to copy to clipboard'
  }
}

const isExpired = (key: ApiKey): boolean => {
  if (!key.expiresAt) return false
  return new Date(key.expiresAt) < new Date()
}

const formatDate = (dateStr: string): string => {
  const date = new Date(dateStr)
  return date.toLocaleDateString() + ' ' + date.toLocaleTimeString()
}

// Lifecycle
onMounted(() => {
  loadApiKeys()
})
</script>

<style scoped>
.api-key-management-view {
  padding: 20px;
  max-width: 1400px;
  margin: 0 auto;
}

.header {
  margin-bottom: 30px;
}

.header h2 {
  margin: 0 0 10px 0;
  font-size: 28px;
  color: var(--text-primary, #e0e0e0);
}

.subtitle {
  color: var(--text-secondary, #a0a0a0);
  margin: 0;
}

.create-key-section {
  background: var(--bg-secondary, #1a1f2e);
  padding: 25px;
  border-radius: 8px;
  margin-bottom: 30px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.create-key-section h3 {
  margin-top: 0;
  color: var(--text-primary, #e0e0e0);
}

.form-group {
  margin-bottom: 20px;
}

.form-group label {
  display: block;
  margin-bottom: 8px;
  color: var(--text-primary, #e0e0e0);
  font-weight: 500;
}

.text-input,
.select-input,
.number-input {
  width: 100%;
  max-width: 500px;
  padding: 10px;
  background: var(--bg-primary, #0f1419);
  border: 1px solid var(--border-color, #2a2f3e);
  border-radius: 4px;
  color: var(--text-primary, #e0e0e0);
  font-size: 14px;
}

.number-input {
  max-width: 150px;
}

.rate-limit-options {
  margin-top: 15px;
  padding-left: 25px;
}

.rate-limit-options label {
  display: block;
  margin-bottom: 10px;
}

.primary-button,
.secondary-button,
.danger-button,
.refresh-button,
.copy-button {
  padding: 10px 20px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 14px;
  font-weight: 500;
  transition: all 0.2s;
}

.primary-button {
  background: var(--primary-color, #4c9aff);
  color: white;
}

.primary-button:hover:not(:disabled) {
  background: var(--primary-hover, #3a88e8);
}

.primary-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.secondary-button {
  background: var(--bg-secondary, #1a1f2e);
  color: var(--text-primary, #e0e0e0);
  border: 1px solid var(--border-color, #2a2f3e);
}

.danger-button {
  background: #dc3545;
  color: white;
}

.danger-button:hover {
  background: #c82333;
}

.danger-button.small {
  padding: 5px 10px;
  font-size: 12px;
}

.generated-key-alert {
  background: #1e4620;
  border: 1px solid #2d6a32;
  padding: 20px;
  border-radius: 8px;
  margin-bottom: 30px;
}

.generated-key-alert h3 {
  margin-top: 0;
  color: #4ade80;
}

.warning {
  color: #fbbf24;
  font-weight: 600;
  margin: 10px 0;
}

.key-display {
  background: var(--bg-primary, #0f1419);
  padding: 15px;
  border-radius: 4px;
  display: flex;
  align-items: center;
  gap: 10px;
  margin: 15px 0;
}

.key-display code {
  flex: 1;
  color: #4ade80;
  font-size: 14px;
  word-break: break-all;
}

.copy-button {
  background: var(--primary-color, #4c9aff);
  color: white;
  padding: 8px 15px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  white-space: nowrap;
}

.error-message {
  background: #4a1a1a;
  border: 1px solid #8a2a2a;
  padding: 15px;
  border-radius: 4px;
  margin-bottom: 20px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  color: #ff6b6b;
}

.close-button {
  background: none;
  border: none;
  color: #ff6b6b;
  font-size: 24px;
  cursor: pointer;
  padding: 0;
  width: 30px;
  height: 30px;
}

.keys-list-section {
  margin-bottom: 30px;
}

.section-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
}

.section-header h3 {
  margin: 0;
  color: var(--text-primary, #e0e0e0);
}

.refresh-button {
  background: var(--bg-secondary, #1a1f2e);
  color: var(--text-primary, #e0e0e0);
  border: 1px solid var(--border-color, #2a2f3e);
}

.loading-state,
.empty-state {
  text-align: center;
  padding: 40px;
  color: var(--text-secondary, #a0a0a0);
}

.spinner {
  border: 3px solid var(--bg-secondary, #1a1f2e);
  border-top: 3px solid var(--primary-color, #4c9aff);
  border-radius: 50%;
  width: 40px;
  height: 40px;
  animation: spin 1s linear infinite;
  margin: 0 auto 20px;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

.keys-table {
  overflow-x: auto;
}

.keys-table table {
  width: 100%;
  border-collapse: collapse;
  background: var(--bg-secondary, #1a1f2e);
  border-radius: 8px;
  overflow: hidden;
}

.keys-table th,
.keys-table td {
  padding: 12px;
  text-align: left;
  border-bottom: 1px solid var(--border-color, #2a2f3e);
}

.keys-table th {
  background: var(--bg-primary, #0f1419);
  color: var(--text-primary, #e0e0e0);
  font-weight: 600;
}

.keys-table td {
  color: var(--text-secondary, #a0a0a0);
}

.keys-table tr.expired {
  opacity: 0.6;
}

.key-preview {
  font-family: 'Courier New', monospace;
  font-size: 12px;
  color: var(--text-primary, #e0e0e0);
}

.role-badge {
  padding: 4px 8px;
  border-radius: 4px;
  font-size: 12px;
  font-weight: 600;
  text-transform: uppercase;
}

.role-admin {
  background: #dc3545;
  color: white;
}

.role-analyst {
  background: #ffc107;
  color: #000;
}

.role-viewer {
  background: #6c757d;
  color: white;
}

.status-badge {
  padding: 4px 8px;
  border-radius: 4px;
  font-size: 12px;
  font-weight: 600;
}

.status-active {
  background: #28a745;
  color: white;
}

.status-expired,
.status-revoked {
  background: #6c757d;
  color: white;
}

.never {
  color: var(--text-tertiary, #707070);
  font-style: italic;
}

.action-buttons {
  display: flex;
  gap: 8px;
}

.statistics-section {
  background: var(--bg-secondary, #1a1f2e);
  padding: 25px;
  border-radius: 8px;
  margin-bottom: 30px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.statistics-section h3 {
  margin-top: 0;
  color: var(--text-primary, #e0e0e0);
}

.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 20px;
  margin-top: 20px;
}

.stat-card {
  background: var(--bg-primary, #0f1419);
  padding: 20px;
  border-radius: 8px;
  text-align: center;
  border: 1px solid var(--border-color, #2a2f3e);
}

.stat-value {
  font-size: 32px;
  font-weight: 700;
  color: var(--primary-color, #4c9aff);
  margin-bottom: 8px;
}

.stat-label {
  color: var(--text-secondary, #a0a0a0);
  font-size: 14px;
}

.documentation-section {
  background: var(--bg-secondary, #1a1f2e);
  padding: 25px;
  border-radius: 8px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.documentation-section h3 {
  margin-top: 0;
  color: var(--text-primary, #e0e0e0);
}

.documentation-section h4 {
  color: var(--text-primary, #e0e0e0);
  margin-top: 20px;
}

.doc-content {
  color: var(--text-secondary, #a0a0a0);
}

.doc-content pre {
  background: var(--bg-primary, #0f1419);
  padding: 15px;
  border-radius: 4px;
  overflow-x: auto;
  border: 1px solid var(--border-color, #2a2f3e);
}

.doc-content code {
  color: var(--primary-color, #4c9aff);
  font-family: 'Courier New', monospace;
}

.doc-content ul {
  margin-top: 10px;
}

.doc-content li {
  margin-bottom: 8px;
}
</style>
