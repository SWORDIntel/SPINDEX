import axios from 'axios'

const API_URL = '/api' // This should be configured in a more robust way

export const getRoles = () => {
  return axios.get(`${API_URL}/roles`)
}

export const createRole = (role: any) => {
  return axios.post(`${API_URL}/roles`, role)
}

export const updateRole = (role: any) => {
  return axios.put(`${API_URL}/roles/${role.id}`, role)
}

export const deleteRole = (roleId: string) => {
  return axios.delete(`${API_URL}/roles/${roleId}`)
}

export const getPermissions = () => {
  return axios.get(`${API_URL}/permissions`)
}

export const getRolePermissions = (roleId: string) => {
  return axios.get(`${API_URL}/roles/${roleId}/permissions`)
}

export const updateRolePermissions = (roleId: string, permissions: any) => {
  return axios.put(`${API_URL}/roles/${roleId}/permissions`, permissions)
}

export const getUsers = () => {
  return axios.get(`${API_URL}/users`)
}

export const getUserRoles = (userId: string) => {
  return axios.get(`${API_URL}/users/${userId}/roles`)
}

export const updateUserRoles = (userId: string, roles: any) => {
  return axios.put(`${API_URL}/users/${userId}/roles`, roles)
}
