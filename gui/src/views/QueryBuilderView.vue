<template>
  <div class="query-builder-view">
    <div class="view-header">
      <h2>🔨 Query Builder</h2>
      <p class="subtitle">Build complex search queries visually, no SQL required</p>
    </div>

    <!-- Mode Toggle -->
    <div class="mode-toggle">
      <button
        @click="mode = 'builder'"
        :class="['mode-button', { active: mode === 'builder' }]"
      >
        Visual Builder
      </button>
      <button
        @click="mode = 'raw'"
        :class="['mode-button', { active: mode === 'raw' }]"
      >
        Raw JSON Editor
      </button>
    </div>

    <!-- Visual Query Builder -->
    <div v-if="mode === 'builder'" class="builder-container">
      <QueryBuilder @execute="handleExecute" @save="handleSave" />
    </div>

    <!-- Raw JSON Editor -->
    <div v-else class="raw-editor-container">
      <div class="editor-section">
        <h3>Raw Query Editor</h3>
        <p class="help-text">Edit the Elasticsearch query JSON directly</p>
        <textarea
          v-model="rawQuery"
          class="raw-query-editor"
          rows="20"
          placeholder='{\n  "query": {\n    "match_all": {}\n  }\n}'
        ></textarea>
        <div class="editor-actions">
          <button @click="formatQuery" class="secondary-button">Format JSON</button>
          <button @click="validateQuery" class="secondary-button">Validate</button>
          <button @click="executeRawQuery" class="primary-button">Execute Query</button>
        </div>
        <div v-if="validationError" class="error-message">
          <strong>Validation Error:</strong> {{ validationError }}
        </div>
      </div>
    </div>

    <!-- Search Results -->
    <div v-if="searchResults !== null" class="results-section">
      <div class="results-header">
        <h3>Search Results</h3>
        <div class="results-meta">
          <span class="result-count">{{ searchResults.total }} results</span>
          <span class="execution-time">in {{ searchResults.took }}ms</span>
        </div>
      </div>

      <div v-if="searchResults.hits.length === 0" class="empty-results">
        <p>No results found. Try adjusting your query.</p>
      </div>

      <div v-else class="results-list">
        <div v-for="(hit, index) in searchResults.hits" :key="index" class="result-item">
          <div class="result-header">
            <h4>{{ hit._source.title || 'Untitled' }}</h4>
            <span class="result-score">Score: {{ hit._score }}</span>
          </div>
          <div class="result-body">
            <p v-if="hit._source.description">{{ hit._source.description }}</p>
            <p v-else-if="hit._source.content">{{ truncate(hit._source.content, 200) }}</p>
          </div>
          <div class="result-meta">
            <span v-if="hit._source.author" class="meta-item">
              <strong>Author:</strong> {{ hit._source.author }}
            </span>
            <span v-if="hit._source.created_date" class="meta-item">
              <strong>Created:</strong> {{ formatDate(hit._source.created_date) }}
            </span>
            <span class="meta-item">
              <strong>Index:</strong> {{ hit._index }}
            </span>
          </div>
          <button @click="viewDetails(hit)" class="details-button">View Details</button>
        </div>
      </div>

      <div v-if="searchResults.total > searchResults.hits.length" class="pagination">
        <button @click="loadMore" class="secondary-button">Load More Results</button>
      </div>
    </div>

    <!-- Saved Queries -->
    <div class="saved-queries-section">
      <div class="section-header">
        <h3>💾 Saved Queries</h3>
        <button @click="showSavedQueries = !showSavedQueries" class="toggle-button">
          {{ showSavedQueries ? '▲ Hide' : '▼ Show' }}
        </button>
      </div>

      <div v-if="showSavedQueries" class="saved-queries-list">
        <div v-if="savedQueries.length === 0" class="empty-state">
          <p>No saved queries yet. Build a query and click "Save Query" to save it.</p>
        </div>
        <div v-else class="queries-grid">
          <div v-for="(query, index) in savedQueries" :key="index" class="saved-query-card">
            <div class="card-header">
              <h4>{{ query.name }}</h4>
              <button @click="deleteSavedQuery(index)" class="delete-button">×</button>
            </div>
            <div class="card-body">
              <p class="query-description">{{ query.description || 'No description' }}</p>
              <span class="query-date">Saved: {{ formatDate(query.savedAt) }}</span>
            </div>
            <div class="card-actions">
              <button @click="loadSavedQuery(query)" class="secondary-button small">Load</button>
              <button @click="executeSavedQuery(query)" class="primary-button small">Execute</button>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import QueryBuilder from '@/components/search/QueryBuilder.vue'
import axios from 'axios'

// Types
interface SearchResults {
  total: number
  took: number
  hits: any[]
}

interface SavedQuery {
  name: string
  description: string
  query: any
  savedAt: string
}

// State
const mode = ref<'builder' | 'raw'>('builder')
const rawQuery = ref('')
const searchResults = ref<SearchResults | null>(null)
const validationError = ref('')
const showSavedQueries = ref(false)
const savedQueries = ref<SavedQuery[]>([])

// Load saved queries from localStorage
const loadSavedQueriesFromStorage = () => {
  const stored = localStorage.getItem('spindex_saved_queries')
  if (stored) {
    try {
      savedQueries.value = JSON.parse(stored)
    } catch (e) {
      console.error('Failed to load saved queries:', e)
    }
  }
}

// Save queries to localStorage
const saveSavedQueriesToStorage = () => {
  localStorage.setItem('spindex_saved_queries', JSON.stringify(savedQueries.value))
}

// Initialize
loadSavedQueriesFromStorage()

// Methods
const handleExecute = async (query: any) => {
  try {
    // Mock execution - replace with actual API call
    // const response = await axios.post('/api/search', query)

    // Mock data
    await new Promise(resolve => setTimeout(resolve, 500))
    searchResults.value = {
      total: 42,
      took: 23,
      hits: [
        {
          _index: 'spindex_main',
          _score: 1.5,
          _source: {
            title: 'Sample Document 1',
            description: 'This is a sample document that matches your query.',
            author: 'John Doe',
            created_date: new Date().toISOString()
          }
        },
        {
          _index: 'spindex_main',
          _score: 1.2,
          _source: {
            title: 'Sample Document 2',
            content: 'This document contains relevant information about spatial indexing and search optimization.',
            author: 'Jane Smith',
            created_date: new Date(Date.now() - 86400000).toISOString()
          }
        }
      ]
    }
  } catch (error: any) {
    validationError.value = error.message || 'Failed to execute query'
  }
}

const handleSave = (queryData: any) => {
  const name = prompt('Enter a name for this query:')
  if (!name) return

  const description = prompt('Enter a description (optional):') || ''

  const savedQuery: SavedQuery = {
    name,
    description,
    query: queryData,
    savedAt: new Date().toISOString()
  }

  savedQueries.value.push(savedQuery)
  saveSavedQueriesToStorage()
  showSavedQueries.value = true
}

const executeRawQuery = async () => {
  validationError.value = ''

  try {
    const query = JSON.parse(rawQuery.value)
    await handleExecute(query)
  } catch (error: any) {
    validationError.value = 'Invalid JSON: ' + error.message
  }
}

const formatQuery = () => {
  try {
    const parsed = JSON.parse(rawQuery.value)
    rawQuery.value = JSON.stringify(parsed, null, 2)
    validationError.value = ''
  } catch (error: any) {
    validationError.value = 'Invalid JSON: ' + error.message
  }
}

const validateQuery = () => {
  try {
    JSON.parse(rawQuery.value)
    validationError.value = ''
    alert('✓ Query is valid JSON')
  } catch (error: any) {
    validationError.value = 'Invalid JSON: ' + error.message
  }
}

const loadSavedQuery = (query: SavedQuery) => {
  rawQuery.value = JSON.stringify(query.query.generatedQuery ? JSON.parse(query.query.generatedQuery) : query.query, null, 2)
  mode.value = 'raw'
}

const executeSavedQuery = async (query: SavedQuery) => {
  const queryToExecute = query.query.generatedQuery ? JSON.parse(query.query.generatedQuery) : query.query
  await handleExecute(queryToExecute)
}

const deleteSavedQuery = (index: number) => {
  if (confirm('Are you sure you want to delete this saved query?')) {
    savedQueries.value.splice(index, 1)
    saveSavedQueriesToStorage()
  }
}

const viewDetails = (hit: any) => {
  alert(JSON.stringify(hit, null, 2))
  // In production, navigate to detail view
}

const loadMore = () => {
  // Implement pagination
  alert('Load more functionality would fetch additional results')
}

const truncate = (text: string, length: number) => {
  if (text.length <= length) return text
  return text.substring(0, length) + '...'
}

const formatDate = (dateStr: string) => {
  const date = new Date(dateStr)
  return date.toLocaleDateString() + ' ' + date.toLocaleTimeString()
}
</script>

<style scoped>
.query-builder-view {
  padding: 20px;
  max-width: 1600px;
  margin: 0 auto;
}

.view-header {
  margin-bottom: 25px;
}

.view-header h2 {
  margin: 0 0 10px 0;
  font-size: 28px;
  color: var(--text-primary, #e0e0e0);
}

.subtitle {
  color: var(--text-secondary, #a0a0a0);
  margin: 0;
}

.mode-toggle {
  display: flex;
  gap: 0;
  margin-bottom: 25px;
  border-radius: 8px;
  overflow: hidden;
  width: fit-content;
}

.mode-button {
  padding: 10px 24px;
  border: 1px solid var(--border-color, #2a2f3e);
  background: var(--bg-secondary, #1a1f2e);
  color: var(--text-primary, #e0e0e0);
  cursor: pointer;
  font-size: 14px;
  font-weight: 500;
  transition: all 0.2s;
}

.mode-button:first-child {
  border-right: none;
  border-radius: 8px 0 0 8px;
}

.mode-button:last-child {
  border-left: none;
  border-radius: 0 8px 8px 0;
}

.mode-button:hover {
  background: var(--bg-primary, #0f1419);
}

.mode-button.active {
  background: var(--primary-color, #4c9aff);
  color: white;
  border-color: var(--primary-color, #4c9aff);
}

.builder-container,
.raw-editor-container {
  margin-bottom: 30px;
}

.editor-section {
  background: var(--bg-secondary, #1a1f2e);
  padding: 25px;
  border-radius: 8px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.editor-section h3 {
  margin: 0 0 8px 0;
  color: var(--text-primary, #e0e0e0);
}

.help-text {
  color: var(--text-secondary, #a0a0a0);
  font-size: 14px;
  margin-bottom: 15px;
  display: block;
}

.raw-query-editor {
  width: 100%;
  padding: 15px;
  background: var(--bg-primary, #0f1419);
  border: 1px solid var(--border-color, #2a2f3e);
  border-radius: 4px;
  color: var(--text-primary, #e0e0e0);
  font-family: 'Courier New', monospace;
  font-size: 14px;
  resize: vertical;
}

.raw-query-editor:focus {
  outline: none;
  border-color: var(--primary-color, #4c9aff);
}

.editor-actions {
  display: flex;
  gap: 10px;
  margin-top: 15px;
}

.primary-button,
.secondary-button {
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

.primary-button:hover {
  background: var(--primary-hover, #3a88e8);
}

.secondary-button {
  background: var(--bg-primary, #0f1419);
  color: var(--text-primary, #e0e0e0);
  border: 1px solid var(--border-color, #2a2f3e);
}

.secondary-button:hover {
  background: var(--bg-secondary, #1a1f2e);
}

.primary-button.small,
.secondary-button.small {
  padding: 6px 12px;
  font-size: 12px;
}

.error-message {
  margin-top: 15px;
  padding: 12px;
  background: #4a1a1a;
  border: 1px solid #8a2a2a;
  border-radius: 4px;
  color: #ff6b6b;
}

.results-section {
  margin-bottom: 30px;
  background: var(--bg-secondary, #1a1f2e);
  padding: 25px;
  border-radius: 8px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.results-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
}

.results-header h3 {
  margin: 0;
  color: var(--text-primary, #e0e0e0);
}

.results-meta {
  display: flex;
  gap: 15px;
  color: var(--text-secondary, #a0a0a0);
  font-size: 14px;
}

.result-count {
  font-weight: 600;
  color: var(--primary-color, #4c9aff);
}

.empty-results {
  text-align: center;
  padding: 40px;
  color: var(--text-secondary, #a0a0a0);
}

.results-list {
  display: flex;
  flex-direction: column;
  gap: 15px;
}

.result-item {
  background: var(--bg-primary, #0f1419);
  padding: 20px;
  border-radius: 8px;
  border: 1px solid var(--border-color, #2a2f3e);
  transition: all 0.2s;
}

.result-item:hover {
  border-color: var(--primary-color, #4c9aff);
}

.result-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 10px;
}

.result-header h4 {
  margin: 0;
  color: var(--text-primary, #e0e0e0);
  font-size: 18px;
}

.result-score {
  color: var(--text-secondary, #a0a0a0);
  font-size: 12px;
  background: var(--bg-secondary, #1a1f2e);
  padding: 4px 8px;
  border-radius: 4px;
}

.result-body {
  margin-bottom: 15px;
  color: var(--text-secondary, #a0a0a0);
  line-height: 1.6;
}

.result-meta {
  display: flex;
  gap: 15px;
  margin-bottom: 15px;
  flex-wrap: wrap;
}

.meta-item {
  font-size: 13px;
  color: var(--text-secondary, #a0a0a0);
}

.meta-item strong {
  color: var(--text-primary, #e0e0e0);
}

.details-button {
  padding: 6px 12px;
  background: var(--primary-color, #4c9aff);
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 12px;
  transition: all 0.2s;
}

.details-button:hover {
  background: var(--primary-hover, #3a88e8);
}

.pagination {
  margin-top: 20px;
  text-align: center;
}

.saved-queries-section {
  background: var(--bg-secondary, #1a1f2e);
  padding: 25px;
  border-radius: 8px;
  border: 1px solid var(--border-color, #2a2f3e);
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

.toggle-button {
  padding: 6px 12px;
  background: var(--bg-primary, #0f1419);
  color: var(--text-primary, #e0e0e0);
  border: 1px solid var(--border-color, #2a2f3e);
  border-radius: 4px;
  cursor: pointer;
  font-size: 12px;
}

.empty-state {
  text-align: center;
  padding: 40px;
  color: var(--text-secondary, #a0a0a0);
}

.queries-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
  gap: 15px;
}

.saved-query-card {
  background: var(--bg-primary, #0f1419);
  padding: 15px;
  border-radius: 8px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 10px;
}

.card-header h4 {
  margin: 0;
  color: var(--text-primary, #e0e0e0);
  font-size: 16px;
}

.delete-button {
  background: #dc3545;
  color: white;
  border: none;
  width: 24px;
  height: 24px;
  border-radius: 4px;
  cursor: pointer;
  font-size: 18px;
  line-height: 1;
}

.delete-button:hover {
  background: #c82333;
}

.card-body {
  margin-bottom: 15px;
}

.query-description {
  color: var(--text-secondary, #a0a0a0);
  font-size: 13px;
  margin-bottom: 8px;
}

.query-date {
  color: var(--text-tertiary, #707070);
  font-size: 11px;
}

.card-actions {
  display: flex;
  gap: 8px;
}
</style>
