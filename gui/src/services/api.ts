/**
 * API Service Layer
 * Handles all communication with SPINDEX FastAPI backend
 */

import axios, { AxiosInstance, AxiosError } from 'axios';

// API Configuration
const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || 'http://localhost:8000';
const API_TIMEOUT = 30000; // 30 seconds

// Create axios instance with default config
const apiClient: AxiosInstance = axios.create({
  baseURL: API_BASE_URL,
  timeout: API_TIMEOUT,
  headers: {
    'Content-Type': 'application/json',
  },
});

// Request interceptor - add auth token
apiClient.interceptors.request.use(
  (config) => {
    const token = localStorage.getItem('auth_token');
    if (token) {
      config.headers['X-Auth-Token'] = token;
    }
    return config;
  },
  (error) => {
    return Promise.reject(error);
  }
);

// Response interceptor - handle errors globally
apiClient.interceptors.response.use(
  (response) => response,
  (error: AxiosError) => {
    if (error.response?.status === 401) {
      // Unauthorized - clear token and redirect to login
      localStorage.removeItem('auth_token');
      window.location.href = '/login';
    }
    return Promise.reject(error);
  }
);

// Types for API responses
export interface SearchResult {
  doc_id: string;
  title: string;
  text_content: string;
  relevance_score: number;
  avx_match_count: number;
  avx_matched_positions: number[];
  metadata: Record<string, any>;
}

export interface SearchResponse {
  es_query_time_ms: number;
  avx_processing_time_ms: number;
  total_results_before_avx: number;
  total_results_after_avx: number;
  results: SearchResult[];
}

export interface SearchRequest {
  index_name?: string;
  query_text: string;
  avx_phrases?: string[];
  use_avx?: boolean;
  avx_case_sensitive?: boolean;
  avx_find_all_occurrences?: boolean;
  avx_max_hits_per_doc?: number;
}

export interface IndexMetadata {
  index_id: string;
  index_name: string;
  index_type: string;
  promoted_data_path: string;
  version: number;
  status: 'creating' | 'ready' | 'failed';
  created_at: string;
  updated_at: string;
  celery_task_id?: string;
}

export interface FileUploadResponse {
  job_id: string;
  filename: string;
  message: string;
  staged_filepath: string;
}

export interface IngestionJob {
  job_id: string;
  original_filename: string;
  upload_timestamp: string;
  user_id: string;
  processing_status: string;
  staged_file_path: string;
  pipeline_stages_executed_log: string[];
  validation_summary: Record<string, any>;
  error_log_references: string[];
}

export interface QueryHistoryItem {
  query_id: string;
  timestamp: string;
  user_id: string;
  request_data: {
    index_id?: string;
    query_type: string;
    parameters: Record<string, any>;
  };
}

export interface SystemStats {
  total_indexes: number;
  total_documents: number;
  total_queries_today: number;
  avg_query_time_ms: number;
  active_jobs: number;
  storage_used_gb: number;
}

// API Service Object
export const api = {
  // ==================== SEARCH ====================

  /**
   * Perform search with optional AVX-512 acceleration
   */
  async search(request: SearchRequest): Promise<SearchResponse> {
    const response = await apiClient.post<SearchResponse>('/search_with_avx', request);
    return response.data;
  },

  // ==================== INDEX MANAGEMENT ====================

  /**
   * Get all indexes
   */
  async getIndexes(skip: number = 0, limit: number = 100): Promise<IndexMetadata[]> {
    const response = await apiClient.get<IndexMetadata[]>('/indexes', {
      params: { skip, limit },
    });
    return response.data;
  },

  /**
   * Get specific index by ID
   */
  async getIndex(indexId: string): Promise<IndexMetadata> {
    const response = await apiClient.get<IndexMetadata>(`/indexes/${indexId}`);
    return response.data;
  },

  /**
   * Create new index
   */
  async createIndex(metadata: Partial<IndexMetadata>): Promise<IndexMetadata> {
    const response = await apiClient.post<IndexMetadata>('/indexes', metadata);
    return response.data;
  },

  /**
   * Update index metadata
   */
  async updateIndex(indexId: string, updates: Partial<IndexMetadata>): Promise<IndexMetadata> {
    const response = await apiClient.put<IndexMetadata>(`/indexes/${indexId}`, updates);
    return response.data;
  },

  /**
   * Delete index
   */
  async deleteIndex(indexId: string): Promise<void> {
    await apiClient.delete(`/indexes/${indexId}`);
  },

  // ==================== FILE UPLOAD (AEGIS PIPELINE) ====================

  /**
   * Upload file to AEGIS pipeline
   */
  async uploadFile(file: File, onProgress?: (progress: number) => void): Promise<FileUploadResponse> {
    const formData = new FormData();
    formData.append('file', file);

    const response = await apiClient.post<FileUploadResponse>('/aegis/upload', formData, {
      headers: {
        'Content-Type': 'multipart/form-data',
      },
      onUploadProgress: (progressEvent) => {
        if (onProgress && progressEvent.total) {
          const percentCompleted = Math.round((progressEvent.loaded * 100) / progressEvent.total);
          onProgress(percentCompleted);
        }
      },
    });

    return response.data;
  },

  /**
   * Get ingestion job status
   */
  async getIngestionJob(jobId: string): Promise<IngestionJob> {
    // Note: This endpoint might need to be added to backend
    const response = await apiClient.get<IngestionJob>(`/aegis/jobs/${jobId}`);
    return response.data;
  },

  /**
   * List all ingestion jobs
   */
  async getIngestionJobs(skip: number = 0, limit: number = 50): Promise<IngestionJob[]> {
    // Note: This endpoint might need to be added to backend
    const response = await apiClient.get<IngestionJob[]>('/aegis/jobs', {
      params: { skip, limit },
    });
    return response.data;
  },

  // ==================== QUERY HISTORY ====================

  /**
   * Get user's query history
   */
  async getQueryHistory(skip: number = 0, limit: number = 100): Promise<QueryHistoryItem[]> {
    const response = await apiClient.get<QueryHistoryItem[]>('/users/me/query_history', {
      params: { skip, limit },
    });
    return response.data;
  },

  /**
   * Log a query
   */
  async logQuery(queryData: {
    index_id?: string;
    query_type: string;
    parameters: Record<string, any>;
  }): Promise<QueryHistoryItem> {
    const response = await apiClient.post<QueryHistoryItem>('/users/me/query_history', queryData);
    return response.data;
  },

  // ==================== SYSTEM / DASHBOARD ====================

  /**
   * Get system statistics for dashboard
   * Note: This is a mock implementation - backend endpoint needs to be added
   */
  async getSystemStats(): Promise<SystemStats> {
    try {
      // Try to fetch real stats from backend
      const response = await apiClient.get<SystemStats>('/system/stats');
      return response.data;
    } catch (error) {
      // If endpoint doesn't exist yet, return mock data
      console.warn('System stats endpoint not found, using mock data');
      return {
        total_indexes: 0,
        total_documents: 0,
        total_queries_today: 0,
        avg_query_time_ms: 0,
        active_jobs: 0,
        storage_used_gb: 0,
      };
    }
  },

  // ==================== AUTHENTICATION ====================

  /**
   * Login (simplified - adjust based on actual auth implementation)
   */
  async login(username: string, password: string): Promise<{ token: string; user: any }> {
    const response = await apiClient.post('/auth/login', { username, password });
    const { token, user } = response.data;
    localStorage.setItem('auth_token', token);
    return { token, user };
  },

  /**
   * Logout
   */
  async logout(): Promise<void> {
    localStorage.removeItem('auth_token');
    // Optional: Call backend logout endpoint if it exists
    try {
      await apiClient.post('/auth/logout');
    } catch (error) {
      // Ignore errors on logout
    }
  },

  /**
   * Get current user info
   */
  async getCurrentUser(): Promise<any> {
    const response = await apiClient.get('/users/me');
    return response.data;
  },
};

// Export axios instance for custom requests
export { apiClient };

// Export error handler utility
export function handleApiError(error: any): string {
  if (axios.isAxiosError(error)) {
    if (error.response) {
      // Server responded with error
      return error.response.data?.detail || error.response.data?.message || 'Server error occurred';
    } else if (error.request) {
      // Request made but no response
      return 'Unable to reach server. Please check your connection.';
    }
  }
  return error.message || 'An unexpected error occurred';
}
