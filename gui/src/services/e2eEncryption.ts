import axios from 'axios'

const API_URL = '/api' // This should be configured in a more robust way

export const getKeys = () => {
  return axios.get(`${API_URL}/e2e-encryption/keys`)
}

export const createKey = (key: any) => {
  return axios.post(`${API_URL}/e2e-encryption/keys`, key)
}

export const activateKey = (keyId: string) => {
  return axios.post(`${API_URL}/e2e-encryption/keys/${keyId}/activate`)
}

export const deleteKey = (keyId: string) => {
  return axios.delete(`${API_URL}/e2e-encryption/keys/${keyId}`)
}

export const rekeyData = (keyId: string) => {
  return axios.post(`${API_URL}/e2e-encryption/rekey`, { keyId })
}
