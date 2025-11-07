<template>
  <div class="query-builder">
    <div class="header">
      <h3>🔨 Visual Query Builder</h3>
      <p class="subtitle">Build complex search queries with a visual interface</p>
    </div>

    <!-- Query Type Selection -->
    <div class="query-type-section">
      <label>Query Type</label>
      <div class="button-group">
        <button
          v-for="type in queryTypes"
          :key="type.value"
          @click="queryType = type.value"
          :class="['type-button', { active: queryType === type.value }]"
        >
          {{ type.label }}
        </button>
      </div>
    </div>

    <!-- Conditions -->
    <div class="conditions-section">
      <div class="section-header">
        <h4>Conditions</h4>
        <button @click="addCondition" class="add-button">+ Add Condition</button>
      </div>

      <div v-if="conditions.length === 0" class="empty-state">
        <p>No conditions added. Click "+ Add Condition" to start building your query.</p>
      </div>

      <div v-else class="conditions-list">
        <div v-for="(condition, index) in conditions" :key="index" class="condition-row">
          <!-- Boolean Operator (not shown for first condition) -->
          <div v-if="index > 0" class="operator-select">
            <select v-model="condition.operator" class="select-input">
              <option value="AND">AND</option>
              <option value="OR">OR</option>
              <option value="NOT">NOT</option>
            </select>
          </div>

          <!-- Field Selection -->
          <div class="field-select">
            <select v-model="condition.field" class="select-input" @change="onFieldChange(condition)">
              <option value="">Select Field</option>
              <optgroup label="Text Fields">
                <option value="title">Title</option>
                <option value="content">Content</option>
                <option value="description">Description</option>
                <option value="tags">Tags</option>
              </optgroup>
              <optgroup label="Spatial Fields">
                <option value="location">Location</option>
                <option value="geometry">Geometry</option>
                <option value="coordinates">Coordinates</option>
              </optgroup>
              <optgroup label="Metadata">
                <option value="author">Author</option>
                <option value="created_date">Created Date</option>
                <option value="modified_date">Modified Date</option>
                <option value="status">Status</option>
              </optgroup>
            </select>
          </div>

          <!-- Comparison Operator -->
          <div class="comparison-select">
            <select v-model="condition.comparison" class="select-input">
              <option v-for="comp in getAvailableComparisons(condition)" :key="comp.value" :value="comp.value">
                {{ comp.label }}
              </option>
            </select>
          </div>

          <!-- Value Input -->
          <div class="value-input">
            <input
              v-if="condition.fieldType === 'text'"
              v-model="condition.value"
              type="text"
              placeholder="Enter value..."
              class="text-input"
            />
            <input
              v-else-if="condition.fieldType === 'number'"
              v-model.number="condition.value"
              type="number"
              placeholder="Enter number..."
              class="text-input"
            />
            <input
              v-else-if="condition.fieldType === 'date'"
              v-model="condition.value"
              type="date"
              class="text-input"
            />
            <input
              v-else
              v-model="condition.value"
              type="text"
              placeholder="Enter value..."
              class="text-input"
            />
          </div>

          <!-- Remove Button -->
          <button @click="removeCondition(index)" class="remove-button">×</button>
        </div>
      </div>
    </div>

    <!-- Spatial Filters (Optional) -->
    <div class="spatial-section">
      <div class="section-header">
        <h4>Spatial Filters</h4>
        <label class="toggle-label">
          <input type="checkbox" v-model="spatialFilter.enabled" />
          Enable Spatial Filtering
        </label>
      </div>

      <div v-if="spatialFilter.enabled" class="spatial-options">
        <div class="form-group">
          <label>Filter Type</label>
          <select v-model="spatialFilter.type" class="select-input">
            <option value="bbox">Bounding Box</option>
            <option value="radius">Radius</option>
            <option value="polygon">Polygon</option>
          </select>
        </div>

        <!-- Bounding Box -->
        <div v-if="spatialFilter.type === 'bbox'" class="bbox-inputs">
          <div class="input-row">
            <div class="form-group">
              <label>Min Longitude</label>
              <input v-model.number="spatialFilter.bbox.minLon" type="number" step="0.000001" class="text-input" />
            </div>
            <div class="form-group">
              <label>Min Latitude</label>
              <input v-model.number="spatialFilter.bbox.minLat" type="number" step="0.000001" class="text-input" />
            </div>
          </div>
          <div class="input-row">
            <div class="form-group">
              <label>Max Longitude</label>
              <input v-model.number="spatialFilter.bbox.maxLon" type="number" step="0.000001" class="text-input" />
            </div>
            <div class="form-group">
              <label>Max Latitude</label>
              <input v-model.number="spatialFilter.bbox.maxLat" type="number" step="0.000001" class="text-input" />
            </div>
          </div>
        </div>

        <!-- Radius -->
        <div v-if="spatialFilter.type === 'radius'" class="radius-inputs">
          <div class="input-row">
            <div class="form-group">
              <label>Center Longitude</label>
              <input v-model.number="spatialFilter.radius.lon" type="number" step="0.000001" class="text-input" />
            </div>
            <div class="form-group">
              <label>Center Latitude</label>
              <input v-model.number="spatialFilter.radius.lat" type="number" step="0.000001" class="text-input" />
            </div>
            <div class="form-group">
              <label>Radius (km)</label>
              <input v-model.number="spatialFilter.radius.distance" type="number" step="0.1" class="text-input" />
            </div>
          </div>
        </div>

        <!-- Polygon -->
        <div v-if="spatialFilter.type === 'polygon'" class="polygon-inputs">
          <label>Polygon Coordinates (WKT format)</label>
          <textarea
            v-model="spatialFilter.polygon.wkt"
            placeholder="POLYGON((lon1 lat1, lon2 lat2, lon3 lat3, lon1 lat1))"
            rows="3"
            class="text-input"
          ></textarea>
        </div>
      </div>
    </div>

    <!-- Advanced Options -->
    <div class="advanced-section">
      <button @click="showAdvanced = !showAdvanced" class="toggle-button">
        {{ showAdvanced ? '▲ Hide Advanced Options' : '▼ Show Advanced Options' }}
      </button>

      <div v-if="showAdvanced" class="advanced-options">
        <div class="form-group">
          <label>Result Limit</label>
          <input v-model.number="advanced.limit" type="number" min="1" max="10000" class="text-input" />
        </div>

        <div class="form-group">
          <label>Sort By</label>
          <select v-model="advanced.sortBy" class="select-input">
            <option value="">Relevance (default)</option>
            <option value="created_date">Created Date</option>
            <option value="modified_date">Modified Date</option>
            <option value="title">Title</option>
          </select>
        </div>

        <div class="form-group">
          <label>Sort Order</label>
          <select v-model="advanced.sortOrder" class="select-input">
            <option value="desc">Descending</option>
            <option value="asc">Ascending</option>
          </select>
        </div>

        <div class="form-group">
          <label>
            <input type="checkbox" v-model="advanced.fuzzy" />
            Enable Fuzzy Matching
          </label>
        </div>

        <div class="form-group" v-if="advanced.fuzzy">
          <label>Fuzziness Level</label>
          <select v-model="advanced.fuzziness" class="select-input">
            <option value="AUTO">AUTO</option>
            <option value="1">1</option>
            <option value="2">2</option>
          </select>
        </div>
      </div>
    </div>

    <!-- Generated Query Display -->
    <div class="query-output-section">
      <h4>Generated Query</h4>
      <div class="query-display">
        <pre><code>{{ generatedQuery }}</code></pre>
        <button @click="copyQuery" class="copy-button">
          {{ copied ? '✓ Copied' : 'Copy Query' }}
        </button>
      </div>
    </div>

    <!-- Actions -->
    <div class="actions">
      <button @click="executeQuery" :disabled="!isValid" class="primary-button">
        Execute Query
      </button>
      <button @click="saveQuery" class="secondary-button">
        Save Query
      </button>
      <button @click="resetQuery" class="secondary-button">
        Reset
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, watch } from 'vue'

// Types
interface Condition {
  operator: 'AND' | 'OR' | 'NOT'
  field: string
  fieldType: 'text' | 'number' | 'date' | 'spatial'
  comparison: string
  value: any
}

interface SpatialFilter {
  enabled: boolean
  type: 'bbox' | 'radius' | 'polygon'
  bbox: {
    minLon: number | null
    minLat: number | null
    maxLon: number | null
    maxLat: number | null
  }
  radius: {
    lon: number | null
    lat: number | null
    distance: number | null
  }
  polygon: {
    wkt: string
  }
}

interface Advanced {
  limit: number
  sortBy: string
  sortOrder: 'asc' | 'desc'
  fuzzy: boolean
  fuzziness: string
}

// Props & Emits
const emit = defineEmits(['execute', 'save'])

// State
const queryType = ref<'match' | 'bool' | 'range'>('bool')
const conditions = ref<Condition[]>([])
const spatialFilter = ref<SpatialFilter>({
  enabled: false,
  type: 'bbox',
  bbox: { minLon: null, minLat: null, maxLon: null, maxLat: null },
  radius: { lon: null, lat: null, distance: null },
  polygon: { wkt: '' }
})
const advanced = ref<Advanced>({
  limit: 100,
  sortBy: '',
  sortOrder: 'desc',
  fuzzy: false,
  fuzziness: 'AUTO'
})
const showAdvanced = ref(false)
const copied = ref(false)

// Constants
const queryTypes = [
  { label: 'Boolean Query', value: 'bool' },
  { label: 'Match Query', value: 'match' },
  { label: 'Range Query', value: 'range' }
]

const fieldTypes: Record<string, 'text' | 'number' | 'date' | 'spatial'> = {
  title: 'text',
  content: 'text',
  description: 'text',
  tags: 'text',
  author: 'text',
  location: 'spatial',
  geometry: 'spatial',
  coordinates: 'spatial',
  created_date: 'date',
  modified_date: 'date',
  status: 'text'
}

const textComparisons = [
  { label: 'Contains', value: 'contains' },
  { label: 'Equals', value: 'equals' },
  { label: 'Starts With', value: 'starts_with' },
  { label: 'Ends With', value: 'ends_with' },
  { label: 'Regex', value: 'regex' }
]

const numberComparisons = [
  { label: 'Equals', value: 'equals' },
  { label: 'Greater Than', value: 'gt' },
  { label: 'Greater Than or Equal', value: 'gte' },
  { label: 'Less Than', value: 'lt' },
  { label: 'Less Than or Equal', value: 'lte' }
]

const dateComparisons = [
  { label: 'Equals', value: 'equals' },
  { label: 'After', value: 'gt' },
  { label: 'Before', value: 'lt' },
  { label: 'Between', value: 'range' }
]

const spatialComparisons = [
  { label: 'Within', value: 'within' },
  { label: 'Intersects', value: 'intersects' },
  { label: 'Contains', value: 'contains' }
]

// Computed
const isValid = computed(() => {
  return conditions.value.length > 0 && conditions.value.every(c => c.field && c.value)
})

const generatedQuery = computed(() => {
  const query: any = {
    query: {},
    size: advanced.value.limit
  }

  // Build conditions
  if (conditions.value.length > 0) {
    const must: any[] = []
    const should: any[] = []
    const mustNot: any[] = []

    conditions.value.forEach((condition, index) => {
      const clause = buildClause(condition)

      if (index === 0 || condition.operator === 'AND') {
        must.push(clause)
      } else if (condition.operator === 'OR') {
        should.push(clause)
      } else if (condition.operator === 'NOT') {
        mustNot.push(clause)
      }
    })

    query.query.bool = {}
    if (must.length > 0) query.query.bool.must = must
    if (should.length > 0) query.query.bool.should = should
    if (mustNot.length > 0) query.query.bool.must_not = mustNot
  }

  // Add spatial filter
  if (spatialFilter.value.enabled) {
    if (!query.query.bool) query.query.bool = {}
    if (!query.query.bool.filter) query.query.bool.filter = []

    const spatialClause = buildSpatialClause()
    if (spatialClause) {
      query.query.bool.filter.push(spatialClause)
    }
  }

  // Add sorting
  if (advanced.value.sortBy) {
    query.sort = [
      { [advanced.value.sortBy]: { order: advanced.value.sortOrder } }
    ]
  }

  return JSON.stringify(query, null, 2)
})

// Methods
const addCondition = () => {
  conditions.value.push({
    operator: 'AND',
    field: '',
    fieldType: 'text',
    comparison: 'contains',
    value: ''
  })
}

const removeCondition = (index: number) => {
  conditions.value.splice(index, 1)
}

const onFieldChange = (condition: Condition) => {
  condition.fieldType = fieldTypes[condition.field] || 'text'
  condition.comparison = getAvailableComparisons(condition)[0]?.value || 'contains'
}

const getAvailableComparisons = (condition: Condition) => {
  switch (condition.fieldType) {
    case 'number':
      return numberComparisons
    case 'date':
      return dateComparisons
    case 'spatial':
      return spatialComparisons
    default:
      return textComparisons
  }
}

const buildClause = (condition: Condition) => {
  const { field, comparison, value, fieldType } = condition

  if (advanced.value.fuzzy && fieldType === 'text') {
    return {
      fuzzy: {
        [field]: {
          value: value,
          fuzziness: advanced.value.fuzziness
        }
      }
    }
  }

  switch (comparison) {
    case 'contains':
      return { match: { [field]: value } }
    case 'equals':
      return { term: { [field]: value } }
    case 'starts_with':
      return { prefix: { [field]: value } }
    case 'regex':
      return { regexp: { [field]: value } }
    case 'gt':
      return { range: { [field]: { gt: value } } }
    case 'gte':
      return { range: { [field]: { gte: value } } }
    case 'lt':
      return { range: { [field]: { lt: value } } }
    case 'lte':
      return { range: { [field]: { lte: value } } }
    default:
      return { match: { [field]: value } }
  }
}

const buildSpatialClause = () => {
  const { type, bbox, radius, polygon } = spatialFilter.value

  switch (type) {
    case 'bbox':
      if (bbox.minLon !== null && bbox.minLat !== null && bbox.maxLon !== null && bbox.maxLat !== null) {
        return {
          geo_bounding_box: {
            location: {
              top_left: { lon: bbox.minLon, lat: bbox.maxLat },
              bottom_right: { lon: bbox.maxLon, lat: bbox.minLat }
            }
          }
        }
      }
      break
    case 'radius':
      if (radius.lon !== null && radius.lat !== null && radius.distance !== null) {
        return {
          geo_distance: {
            distance: `${radius.distance}km`,
            location: { lon: radius.lon, lat: radius.lat }
          }
        }
      }
      break
    case 'polygon':
      if (polygon.wkt) {
        return {
          geo_shape: {
            location: {
              shape: polygon.wkt,
              relation: 'intersects'
            }
          }
        }
      }
      break
  }
  return null
}

const executeQuery = () => {
  emit('execute', JSON.parse(generatedQuery.value))
}

const saveQuery = () => {
  const queryData = {
    conditions: conditions.value,
    spatialFilter: spatialFilter.value,
    advanced: advanced.value,
    generatedQuery: generatedQuery.value
  }
  emit('save', queryData)
}

const resetQuery = () => {
  conditions.value = []
  spatialFilter.value = {
    enabled: false,
    type: 'bbox',
    bbox: { minLon: null, minLat: null, maxLon: null, maxLat: null },
    radius: { lon: null, lat: null, distance: null },
    polygon: { wkt: '' }
  }
  advanced.value = {
    limit: 100,
    sortBy: '',
    sortOrder: 'desc',
    fuzzy: false,
    fuzziness: 'AUTO'
  }
  showAdvanced.value = false
}

const copyQuery = async () => {
  try {
    await navigator.clipboard.writeText(generatedQuery.value)
    copied.value = true
    setTimeout(() => {
      copied.value = false
    }, 2000)
  } catch (err) {
    console.error('Failed to copy query:', err)
  }
}
</script>

<style scoped>
.query-builder {
  background: var(--bg-secondary, #1a1f2e);
  padding: 25px;
  border-radius: 8px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.header {
  margin-bottom: 25px;
}

.header h3 {
  margin: 0 0 8px 0;
  color: var(--text-primary, #e0e0e0);
  font-size: 20px;
}

.subtitle {
  margin: 0;
  color: var(--text-secondary, #a0a0a0);
  font-size: 14px;
}

.query-type-section {
  margin-bottom: 25px;
}

.query-type-section label {
  display: block;
  margin-bottom: 10px;
  color: var(--text-primary, #e0e0e0);
  font-weight: 500;
}

.button-group {
  display: flex;
  gap: 10px;
}

.type-button {
  padding: 8px 16px;
  border: 1px solid var(--border-color, #2a2f3e);
  background: var(--bg-primary, #0f1419);
  color: var(--text-primary, #e0e0e0);
  border-radius: 4px;
  cursor: pointer;
  transition: all 0.2s;
}

.type-button:hover {
  background: var(--bg-secondary, #1a1f2e);
}

.type-button.active {
  background: var(--primary-color, #4c9aff);
  color: white;
  border-color: var(--primary-color, #4c9aff);
}

.conditions-section,
.spatial-section,
.advanced-section,
.query-output-section {
  margin-bottom: 25px;
}

.section-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 15px;
}

.section-header h4 {
  margin: 0;
  color: var(--text-primary, #e0e0e0);
  font-size: 16px;
}

.add-button {
  padding: 6px 12px;
  background: var(--primary-color, #4c9aff);
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 14px;
  transition: all 0.2s;
}

.add-button:hover {
  background: var(--primary-hover, #3a88e8);
}

.empty-state {
  text-align: center;
  padding: 30px;
  color: var(--text-secondary, #a0a0a0);
  background: var(--bg-primary, #0f1419);
  border-radius: 4px;
  border: 1px dashed var(--border-color, #2a2f3e);
}

.conditions-list {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.condition-row {
  display: grid;
  grid-template-columns: 80px 1fr 1fr 2fr auto;
  gap: 10px;
  align-items: center;
  padding: 12px;
  background: var(--bg-primary, #0f1419);
  border-radius: 4px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.condition-row:first-child {
  grid-template-columns: 1fr 1fr 2fr auto;
}

.operator-select,
.field-select,
.comparison-select {
  display: flex;
  flex-direction: column;
}

.select-input,
.text-input {
  padding: 8px;
  background: var(--bg-secondary, #1a1f2e);
  border: 1px solid var(--border-color, #2a2f3e);
  border-radius: 4px;
  color: var(--text-primary, #e0e0e0);
  font-size: 14px;
}

.select-input:focus,
.text-input:focus {
  outline: none;
  border-color: var(--primary-color, #4c9aff);
}

.remove-button {
  width: 32px;
  height: 32px;
  background: #dc3545;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 20px;
  line-height: 1;
  transition: all 0.2s;
}

.remove-button:hover {
  background: #c82333;
}

.toggle-label {
  display: flex;
  align-items: center;
  gap: 8px;
  color: var(--text-primary, #e0e0e0);
  cursor: pointer;
}

.toggle-label input[type="checkbox"] {
  cursor: pointer;
}

.spatial-options {
  margin-top: 15px;
  padding: 15px;
  background: var(--bg-primary, #0f1419);
  border-radius: 4px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.form-group {
  margin-bottom: 15px;
}

.form-group label {
  display: block;
  margin-bottom: 6px;
  color: var(--text-primary, #e0e0e0);
  font-size: 14px;
  font-weight: 500;
}

.input-row {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 15px;
}

textarea.text-input {
  width: 100%;
  resize: vertical;
  font-family: 'Courier New', monospace;
}

.toggle-button {
  width: 100%;
  padding: 10px;
  background: var(--bg-primary, #0f1419);
  color: var(--text-primary, #e0e0e0);
  border: 1px solid var(--border-color, #2a2f3e);
  border-radius: 4px;
  cursor: pointer;
  font-size: 14px;
  transition: all 0.2s;
}

.toggle-button:hover {
  background: var(--bg-secondary, #1a1f2e);
}

.advanced-options {
  margin-top: 15px;
  padding: 15px;
  background: var(--bg-primary, #0f1419);
  border-radius: 4px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.query-display {
  position: relative;
  background: var(--bg-primary, #0f1419);
  padding: 15px;
  border-radius: 4px;
  border: 1px solid var(--border-color, #2a2f3e);
}

.query-display pre {
  margin: 0;
  overflow-x: auto;
}

.query-display code {
  color: var(--primary-color, #4c9aff);
  font-family: 'Courier New', monospace;
  font-size: 13px;
}

.copy-button {
  position: absolute;
  top: 10px;
  right: 10px;
  padding: 6px 12px;
  background: var(--primary-color, #4c9aff);
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 12px;
  transition: all 0.2s;
}

.copy-button:hover {
  background: var(--primary-hover, #3a88e8);
}

.actions {
  display: flex;
  gap: 10px;
  margin-top: 25px;
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

.primary-button:hover:not(:disabled) {
  background: var(--primary-hover, #3a88e8);
}

.primary-button:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.secondary-button {
  background: var(--bg-primary, #0f1419);
  color: var(--text-primary, #e0e0e0);
  border: 1px solid var(--border-color, #2a2f3e);
}

.secondary-button:hover {
  background: var(--bg-secondary, #1a1f2e);
}
</style>
