import { shallowMount } from '@vue/test-utils'
import E2EEncryption from '@/views/admin/E2EEncryption.vue'
import {
  getKeys,
  createKey,
  activateKey,
  deleteKey,
  rekeyData
} from '@/services/e2eEncryption'

jest.mock('@/services/e2eEncryption', () => ({
  getKeys: jest.fn(() => Promise.resolve({ data: [] })),
  createKey: jest.fn(() => Promise.resolve({ data: {} })),
  activateKey: jest.fn(() => Promise.resolve()),
  deleteKey: jest.fn(() => Promise.resolve()),
  rekeyData: jest.fn(() => Promise.resolve())
}))

describe('E2EEncryption.vue', () => {
  it('renders correctly', () => {
    const wrapper = shallowMount(E2EEncryption)
    expect(wrapper.exists()).toBe(true)
  })

  it('calls fetchKeys on created', () => {
    const spy = jest.spyOn(E2EEncryption.methods, 'fetchKeys')
    shallowMount(E2EEncryption)
    expect(spy).toHaveBeenCalled()
  })

  it('calls createKey when the form is submitted', async () => {
    const wrapper = shallowMount(E2EEncryption)
    wrapper.vm.newKey.name = 'test'
    wrapper.vm.newKey.description = 'test'
    await wrapper.find('form').trigger('submit.prevent')
    expect(createKey).toHaveBeenCalledWith({ name: 'test', description: 'test' })
  })

  it('calls activateKey when the activate button is clicked', async () => {
    const wrapper = shallowMount(E2EEncryption)
    wrapper.vm.keys = [{ id: '1', name: 'test', description: 'test', active: false }]
    await wrapper.vm.$nextTick()
    await wrapper.find('button.activate').trigger('click')
    expect(activateKey).toHaveBeenCalledWith('1')
  })

  it('calls deleteKey when the delete button is clicked', async () => {
    const wrapper = shallowMount(E2EEncryption)
    wrapper.vm.keys = [{ id: '1', name: 'test', description: 'test', active: false }]
    await wrapper.vm.$nextTick()
    await wrapper.find('button.delete').trigger('click')
    expect(deleteKey).toHaveBeenCalledWith('1')
  })

  it('calls rekeyData when the rekey button is clicked', async () => {
    const wrapper = shallowMount(E2EEncryption)
    wrapper.vm.keys = [{ id: '1', name: 'test', description: 'test', active: true }]
    await wrapper.vm.$nextTick()
    await wrapper.find('button.rekey').trigger('click')
    expect(rekeyData).toHaveBeenCalledWith('1')
  })
})
