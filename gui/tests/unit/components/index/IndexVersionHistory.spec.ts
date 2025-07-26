import { shallowMount } from '@vue/test-utils'
import IndexVersionHistory from '@/components/index/IndexVersionHistory.vue'

describe('IndexVersionHistory.vue', () => {
  it('renders correctly', () => {
    const wrapper = shallowMount(IndexVersionHistory, {
      props: {
        versions: []
      }
    })
    expect(wrapper.exists()).toBe(true)
  })

  it('displays the correct number of versions', () => {
    const versions = [
      { id: '1', version: 1, creationDate: '2025-07-26', dataSource: '/data/1.json', hash: 'abc' },
      { id: '2', version: 2, creationDate: '2025-07-27', dataSource: '/data/2.json', hash: 'def' }
    ]
    const wrapper = shallowMount(IndexVersionHistory, {
      props: {
        versions
      }
    })
    expect(wrapper.findAll('tbody tr')).toHaveLength(2)
  })

  it('calls viewDetails when the Details button is clicked', async () => {
    const spy = jest.spyOn(IndexVersionHistory.methods, 'viewDetails')
    const versions = [{ id: '1', version: 1, creationDate: '2025-07-26', dataSource: '/data/1.json', hash: 'abc' }]
    const wrapper = shallowMount(IndexVersionHistory, {
      props: {
        versions
      }
    })
    await wrapper.find('button.details').trigger('click')
    expect(spy).toHaveBeenCalledWith(versions[0])
  })

  it('calls compareVersion when the Compare button is clicked', async () => {
    const spy = jest.spyOn(IndexVersionHistory.methods, 'compareVersion')
    const versions = [{ id: '1', version: 1, creationDate: '2025-07-26', dataSource: '/data/1.json', hash: 'abc' }]
    const wrapper = shallowMount(IndexVersionHistory, {
      props: {
        versions
      }
    })
    await wrapper.find('button.compare').trigger('click')
    expect(spy).toHaveBeenCalledWith(versions[0])
  })

  it('calls rollbackToVersion when the Rollback button is clicked', async () => {
    const spy = jest.spyOn(IndexVersionHistory.methods, 'rollbackToVersion')
    const versions = [{ id: '1', version: 1, creationDate: '2025-07-26', dataSource: '/data/1.json', hash: 'abc' }]
    const wrapper = shallowMount(IndexVersionHistory, {
      props: {
        versions
      }
    })
    await wrapper.find('button.rollback').trigger('click')
    expect(spy).toHaveBeenCalledWith(versions[0])
  })
})
