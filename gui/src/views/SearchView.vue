<template>
  <div class="search-view">
    <div class="search-header">
      <h2>🔍 Advanced Search</h2>
      <p class="subtitle">Search with AVX-512 acceleration for ultra-fast results</p>
    </div>

    <!-- Search Form -->
    <div class="search-form-container">
      <div class="search-box">
        <input
          v-model="searchQuery"
          @keyup.enter="performSearch"
          type="text"
          placeholder="Enter your search query..."
          class="search-input"
          :disabled="isSearching"
        />
        <button @click="performSearch" :disabled="isSearching || !searchQuery" class="search-button">
          <span v-if="!isSearching">Search</span>
          <span v-else>Searching...</span>
        </button>
      </div>

      <!-- Advanced Options -->
      <div class="advanced-options" v-if="showAdvanced">
        <div class="option-group">
          <label>
            <input type="checkbox" v-model="useAvx" />
            Use AVX-512 Acceleration
          </label>
          <span class="help-text">Enable hardware-accelerated phrase matching (4-10x faster)</span>
        </div>

        <div class="option-group" v-if="useAvx">
          <label for="avx-phrases">AVX Phrase Filters (comma-separated):</label>
          <input
            id="avx-phrases"
            v-model="avxPhrasesInput"
            type="text"
            placeholder="e.g., SPINDEX, performance, spatial"
            class="text-input"
          />
          <span class="help-text">Only return documents containing these exact phrases</span>
        </div>

        <div class="option-row" v-if="useAvx">
          <label>
            <input type="checkbox" v-model="avxCaseSensitive" />
            Case Sensitive
          </label>
          <label>
            <input type="checkbox" v-model="avxFindAll" />
            Find All Occurrences
          </label>
        </div>

        <div class="option-group">
          <label for="index-select">Target Index (optional):</label>
          <select id="index-select" v-model="selectedIndex" class="select-input">
            <option value="">All Indexes</option>
            <option v-for="index in availableIndexes" :key="index.index_id" :value="index.index_name">
              {{ index.index_name }} ({{ index.status }})
            </option>
          </select>
        </div>
      </div>

      <button @click="showAdvanced = !showAdvanced" class="toggle-advanced">
        {{ showAdvanced ? '▲ Hide Advanced Options' : '▼ Show Advanced Options' }}
      </button>
    </div>

    <!-- Error Display -->
    <div v-if="error" class="error-message">
      <strong>Error:</strong> {{ error }}
      <button @click="error = ''" class="close-button">×</button>
    </div>

    <!-- Performance Metrics -->
    <div v-if="searchResults && searchResults.results.length > 0" class="performance-metrics">
      <div class="metric">
        <span class="metric-label">Elasticsearch Time:</span>
        <span class="metric-value">{{ searchResults.es_query_time_ms }}ms</span>
      </div>
      <div class="metric" v-if="useAvx">
        <span class="metric-label">AVX-512 Processing:</span>
        <span class="metric-value">{{ searchResults.avx_processing_time_ms }}ms</span>
      </div>
      <div class="metric">
        <span class="metric-label">Results Before AVX:</span>
        <span class="metric-value">{{ searchResults.total_results_before_avx }}</span>
      </div>
      <div class="metric" v-if="useAvx">
        <span class="metric-label">Results After AVX:</span>
        <span class="metric-value">{{ searchResults.total_results_after_avx }}</span>
      </div>
      <div class="metric speedup" v-if="useAvx && searchResults.total_results_before_avx > 0">
        <span class="metric-label">AVX Speedup:</span>
        <span class="metric-value">{{ calculateSpeedup() }}x faster</span>
      </div>
    </div>

    <!-- Search Results -->
    <div v-if="searchResults" class="results-container">
      <div class="results-header">
        <h3>
          {{ searchResults.results.length }} Results
          <span v-if="useAvx">(AVX-512 Filtered)</span>
        </h3>
        <button @click="exportResults" class="export-button" :disabled="searchResults.results.length === 0">
          📥 Export Results
        </button>
      </div>

      <div v-if="searchResults.results.length === 0" class="no-results">
        <p>No results found matching your query.</p>
        <p class="hint">Try adjusting your search terms or disabling AVX filters.</p>
      </div>

      <div v-else class="results-list">
        <div v-for="(result, index) in searchResults.results" :key="result.doc_id" class="result-item">
          <div class="result-header">
            <h4>{{ result.title || `Document ${result.doc_id}` }}</h4>
            <div class="result-meta">
              <span class="relevance-score" :class="getRelevanceClass(result.relevance_score)">
                Relevance: {{ result.relevance_score.toFixed(2) }}
              </span>
              <span v-if="useAvx && result.avx_match_count > 0" class="avx-matches">
                {{ result.avx_match_count }} AVX matches
              </span>
            </div>
          </div>

          <div class="result-content" v-html="highlightText(result.text_content)"></div>

          <div class="result-footer">
            <span class="doc-id">ID: {{ result.doc_id }}</span>
            <button @click="viewDetails(result)" class="view-button">View Details</button>
          </div>
        </div>
      </div>
    </div>

    <!-- Empty State -->
    <div v-else-if="!isSearching" class="empty-state">
      <div class="empty-icon">🔍</div>
      <h3>Ready to Search</h3>
      <p>Enter a query above to search through indexed documents</p>
      <div class="features-list">
        <div class="feature">✅ Full-text search across all indexes</div>
        <div class="feature">⚡ AVX-512 hardware acceleration</div>
        <div class="feature">🎯 Phrase-level filtering</div>
        <div class="feature">📊 Real-time performance metrics</div>
      </div>
    </div>

    <!-- Loading State -->
    <div v-if="isSearching" class="loading-state">
      <div class="spinner"></div>
      <p>Searching...</p>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, computed } from 'vue';
import { api, SearchResponse, IndexMetadata, handleApiError } from '../services/api';

// State
const searchQuery = ref('');
const avxPhrasesInput = ref('');
const useAvx = ref(true);
const avxCaseSensitive = ref(false);
const avxFindAll = ref(true);
const selectedIndex = ref('');
const showAdvanced = ref(false);

const isSearching = ref(false);
const error = ref('');
const searchResults = ref<SearchResponse | null>(null);
const availableIndexes = ref<IndexMetadata[]>([]);

// Computed
const avxPhrases = computed(() => {
  if (!avxPhrasesInput.value) return [];
  return avxPhrasesInput.value.split(',').map(p => p.trim()).filter(p => p.length > 0);
});

// Methods
async function performSearch() {
  if (!searchQuery.value.trim()) {
    error.value = 'Please enter a search query';
    return;
  }

  isSearching.value = true;
  error.value = '';
  searchResults.value = null;

  try {
    const response = await api.search({
      query_text: searchQuery.value,
      index_name: selectedIndex.value || undefined,
      use_avx: useAvx.value,
      avx_phrases: useAvx.value && avxPhrases.value.length > 0 ? avxPhrases.value : undefined,
      avx_case_sensitive: avxCaseSensitive.value,
      avx_find_all_occurrences: avxFindAll.value,
    });

    searchResults.value = response;

    // Log query to history
    try {
      await api.logQuery({
        query_type: 'search_with_avx',
        parameters: {
          query_text: searchQuery.value,
          use_avx: useAvx.value,
          index_name: selectedIndex.value,
        },
      });
    } catch (e) {
      // Ignore query logging errors
      console.warn('Failed to log query:', e);
    }
  } catch (err: any) {
    error.value = handleApiError(err);
  } finally {
    isSearching.value = false;
  }
}

async function loadIndexes() {
  try {
    const indexes = await api.getIndexes();
    availableIndexes.value = indexes.filter(idx => idx.status === 'ready');
  } catch (err) {
    console.error('Failed to load indexes:', err);
  }
}

function calculateSpeedup(): string {
  if (!searchResults.value) return '0';
  const before = searchResults.value.total_results_before_avx;
  const after = searchResults.value.total_results_after_avx;
  if (after === 0 || before === 0) return '0';
  const speedup = before / after;
  return speedup.toFixed(1);
}

function getRelevanceClass(score: number): string {
  if (score >= 8) return 'high';
  if (score >= 5) return 'medium';
  return 'low';
}

function highlightText(text: string): string {
  if (!text) return '';

  // Limit text length for display
  const maxLength = 500;
  let displayText = text.length > maxLength ? text.substring(0, maxLength) + '...' : text;

  // Highlight search terms
  const terms = searchQuery.value.split(' ').filter(t => t.length > 2);
  terms.forEach(term => {
    const regex = new RegExp(`(${term})`, 'gi');
    displayText = displayText.replace(regex, '<mark>$1</mark>');
  });

  return displayText;
}

function viewDetails(result: any) {
  // TODO: Open modal or navigate to detail view
  console.log('View details for:', result);
  alert(`Document: ${result.doc_id}\n\nRelevance: ${result.relevance_score}\n\n${result.text_content.substring(0, 200)}...`);
}

function exportResults() {
  if (!searchResults.value) return;

  const data = {
    query: searchQuery.value,
    timestamp: new Date().toISOString(),
    total_results: searchResults.value.results.length,
    performance: {
      es_time_ms: searchResults.value.es_query_time_ms,
      avx_time_ms: searchResults.value.avx_processing_time_ms,
    },
    results: searchResults.value.results,
  };

  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = `search-results-${Date.now()}.json`;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

// Lifecycle
onMounted(() => {
  loadIndexes();
});
</script>

<style scoped>
.search-view {
  padding: 30px;
  max-width: 1400px;
  margin: 0 auto;
}

.search-header {
  margin-bottom: 30px;
}

.search-header h2 {
  font-size: 2em;
  margin: 0 0 10px 0;
  color: #2c3e50;
}

.subtitle {
  color: #7f8c8d;
  margin: 0;
}

.search-form-container {
  background: white;
  padding: 25px;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
  margin-bottom: 30px;
}

.search-box {
  display: flex;
  gap: 15px;
  margin-bottom: 20px;
}

.search-input {
  flex: 1;
  padding: 15px 20px;
  font-size: 16px;
  border: 2px solid #e0e0e0;
  border-radius: 8px;
  transition: border-color 0.3s;
}

.search-input:focus {
  outline: none;
  border-color: #3498db;
}

.search-button {
  padding: 15px 40px;
  font-size: 16px;
  font-weight: 600;
  background: linear-gradient(135deg, #3498db, #2980b9);
  color: white;
  border: none;
  border-radius: 8px;
  cursor: pointer;
  transition: transform 0.2s, box-shadow 0.2s;
}

.search-button:hover:not(:disabled) {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(52, 152, 219, 0.3);
}

.search-button:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.toggle-advanced {
  background: none;
  border: none;
  color: #3498db;
  cursor: pointer;
  font-size: 14px;
  padding: 10px 0;
}

.advanced-options {
  padding: 20px;
  background: #f8f9fa;
  border-radius: 8px;
  margin-bottom: 15px;
}

.option-group {
  margin-bottom: 15px;
}

.option-group label {
  display: block;
  font-weight: 600;
  margin-bottom: 8px;
  color: #2c3e50;
}

.option-row {
  display: flex;
  gap: 30px;
  margin-bottom: 15px;
}

.option-row label {
  display: flex;
  align-items: center;
  gap: 8px;
}

.text-input, .select-input {
  width: 100%;
  padding: 10px 15px;
  border: 1px solid #ddd;
  border-radius: 6px;
  font-size: 14px;
}

.help-text {
  display: block;
  font-size: 12px;
  color: #7f8c8d;
  margin-top: 5px;
}

.error-message {
  background: #fee;
  border-left: 4px solid #e74c3c;
  padding: 15px 20px;
  margin-bottom: 20px;
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

.performance-metrics {
  display: flex;
  gap: 20px;
  flex-wrap: wrap;
  padding: 20px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-radius: 12px;
  margin-bottom: 30px;
  color: white;
}

.metric {
  display: flex;
  flex-direction: column;
  gap: 5px;
}

.metric-label {
  font-size: 12px;
  opacity: 0.9;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}

.metric-value {
  font-size: 24px;
  font-weight: 700;
}

.metric.speedup {
  margin-left: auto;
  background: rgba(255,255,255,0.2);
  padding: 10px 20px;
  border-radius: 8px;
}

.results-container {
  background: white;
  border-radius: 12px;
  padding: 25px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
}

.results-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 25px;
  padding-bottom: 15px;
  border-bottom: 2px solid #ecf0f1;
}

.results-header h3 {
  margin: 0;
  color: #2c3e50;
}

.export-button {
  padding: 10px 20px;
  background: #27ae60;
  color: white;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  font-size: 14px;
}

.export-button:hover:not(:disabled) {
  background: #229954;
}

.no-results {
  text-align: center;
  padding: 60px 20px;
  color: #7f8c8d;
}

.no-results .hint {
  font-size: 14px;
  margin-top: 10px;
}

.results-list {
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.result-item {
  border: 1px solid #e0e0e0;
  border-radius: 8px;
  padding: 20px;
  transition: box-shadow 0.2s;
}

.result-item:hover {
  box-shadow: 0 4px 12px rgba(0,0,0,0.1);
}

.result-header {
  display: flex;
  justify-content: space-between;
  align-items: start;
  margin-bottom: 15px;
}

.result-header h4 {
  margin: 0;
  color: #2c3e50;
  flex: 1;
}

.result-meta {
  display: flex;
  gap: 15px;
  align-items: center;
}

.relevance-score {
  padding: 4px 12px;
  border-radius: 12px;
  font-size: 12px;
  font-weight: 600;
}

.relevance-score.high {
  background: #d5f4e6;
  color: #27ae60;
}

.relevance-score.medium {
  background: #fff3cd;
  color: #f39c12;
}

.relevance-score.low {
  background: #f8d7da;
  color: #e74c3c;
}

.avx-matches {
  padding: 4px 12px;
  background: #e3f2fd;
  color: #2196f3;
  border-radius: 12px;
  font-size: 12px;
  font-weight: 600;
}

.result-content {
  line-height: 1.6;
  color: #555;
  margin-bottom: 15px;
}

.result-content :deep(mark) {
  background: #fff3cd;
  padding: 2px 4px;
  border-radius: 3px;
  font-weight: 600;
}

.result-footer {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding-top: 15px;
  border-top: 1px solid #ecf0f1;
}

.doc-id {
  font-size: 12px;
  color: #95a5a6;
  font-family: monospace;
}

.view-button {
  padding: 8px 16px;
  background: #3498db;
  color: white;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  font-size: 13px;
}

.view-button:hover {
  background: #2980b9;
}

.empty-state {
  text-align: center;
  padding: 80px 40px;
  background: white;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.1);
}

.empty-icon {
  font-size: 80px;
  margin-bottom: 20px;
}

.empty-state h3 {
  margin: 0 0 10px 0;
  color: #2c3e50;
}

.empty-state p {
  color: #7f8c8d;
  margin-bottom: 30px;
}

.features-list {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 15px;
  max-width: 800px;
  margin: 0 auto;
}

.feature {
  padding: 15px;
  background: #f8f9fa;
  border-radius: 8px;
  font-size: 14px;
  color: #2c3e50;
}

.loading-state {
  text-align: center;
  padding: 80px 40px;
}

.spinner {
  width: 60px;
  height: 60px;
  border: 6px solid #f3f3f3;
  border-top: 6px solid #3498db;
  border-radius: 50%;
  margin: 0 auto 20px;
  animation: spin 1s linear infinite;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}
</style>
