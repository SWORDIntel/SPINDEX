import { shallowMount } from '@vue/test-utils'
import RBAC from '@/views/admin/RBAC.vue'
import {
  getRoles,
  createRole,
  deleteRole,
  getPermissions,
  updateRolePermissions,
  getUsers,
  updateUserRoles
} from '@/services/rbac'

jest.mock('@/services/rbac', () => ({
  getRoles: jest.fn(() => Promise.resolve({ data: [] })),
  createRole: jest.fn(() => Promise.resolve({ data: {} })),
  deleteRole: jest.fn(() => Promise.resolve()),
  getPermissions: jest.fn(() => Promise.resolve({ data: [] })),
  updateRolePermissions: jest.fn(() => Promise.resolve()),
  getUsers: jest.fn(() => Promise.resolve({ data: [] })),
  updateUserRoles: jest.fn(() => Promise.resolve())
}))

describe('RBAC.vue', () => {
  it('renders correctly', () => {
    const wrapper = shallowMount(RBAC)
    expect(wrapper.exists()).toBe(true)
  })

  it('calls fetchData on created', () => {
    const spy = jest.spyOn(RBAC.methods, 'fetchData')
    shallowMount(RBAC)
    expect(spy).toHaveBeenCalled()
  })

  it('calls createRole when the form is submitted', async () => {
    const wrapper = shallowMount(RBAC)
    wrapper.vm.newRole.name = 'test'
    wrapper.vm.newRole.description = 'test'
    await wrapper.find('form').trigger('submit.prevent')
    expect(createRole).toHaveBeenCalledWith({ name: 'test', description: 'test' })
  })

  it('calls deleteRole when the delete button is clicked', async () => {
    const wrapper = shallowMount(RBAC)
    wrapper.vm.roles = [{ id: '1', name: 'test', description: 'test' }]
    await wrapper.vm.$nextTick()
    await wrapper.find('button.delete').trigger('click')
    expect(deleteRole).toHaveBeenCalledWith('1')
  })

  it('calls updateRolePermissions when a role permission checkbox is clicked', async () => {
    const wrapper = shallowMount(RBAC)
    wrapper.vm.roles = [{ id: '1', name: 'test', description: 'test' }]
    wrapper.vm.permissions = [{ id: '1', name: 'test', description: 'test' }]
    await wrapper.vm.$nextTick()
    await wrapper.find('input[type="checkbox"]').trigger('change')
    expect(updateRolePermissions).toHaveBeenCalled()
  })

  it('calls updateUserRoles when a user role checkbox is clicked', async () => {
    const wrapper = shallowMount(RBAC)
    wrapper.vm.users = [{ id: '1', name: 'test' }]
    wrapper.vm.roles = [{ id: '1', name: 'test', description: 'test' }]
    await wrapper.vm.$nextTick()
    await wrapper.findAll('input[type="checkbox"]')[1].trigger('change')
    expect(updateUserRoles).toHaveBeenCalled()
  })
})
