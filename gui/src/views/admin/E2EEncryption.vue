<template>
  <div>
    <h1>E2E Encryption</h1>
    <div class="e2e-encryption-container">
      <div class="keys-container">
        <h2>Encryption Keys</h2>
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Description</th>
              <th>Creation Date</th>
              <th>Active</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="key in keys" :key="key.id">
              <td>{{ key.name }}</td>
              <td>{{ key.description }}</td>
              <td>{{ key.creationDate }}</td>
              <td>{{ key.active ? 'Yes' : 'No' }}</td>
              <td>
                <button @click="activateKey(key)" :disabled="key.active">Activate</button>
                <button @click="deleteKey(key)" :disabled="key.active">Delete</button>
                <button @click="rekeyData(key)" :disabled="!key.active">Rekey Data</button>
              </td>
            </tr>
          </tbody>
        </table>
        <form @submit.prevent="createKey">
          <h3>Create Key</h3>
          <input v-model="newKey.name" placeholder="Name" required />
          <input v-model="newKey.description" placeholder="Description" required />
          <button type="submit">Create</button>
        </form>
      </div>
    </div>
  </div>
</template>

<script>
import {
  getKeys,
  createKey,
  activateKey,
  deleteKey,
  rekeyData
} from '@/services/e2eEncryption'

export default {
  data() {
    return {
      keys: [],
      newKey: {
        name: '',
        description: ''
      }
    }
  },
  created() {
    this.fetchKeys()
  },
  methods: {
    async fetchKeys() {
      try {
        const res = await getKeys()
        this.keys = res.data
      } catch (error) {
        console.error(error)
      }
    },
    async createKey() {
      try {
        const res = await createKey(this.newKey)
        this.keys.push(res.data)
        this.newKey.name = ''
        this.newKey.description = ''
      } catch (error) {
        console.error(error)
      }
    },
    async activateKey(key) {
      try {
        await activateKey(key.id)
        this.fetchKeys()
      } catch (error) {
        console.error(error)
      }
    },
    async deleteKey(key) {
      try {
        await deleteKey(key.id)
        this.keys = this.keys.filter(k => k.id !== key.id)
      } catch (error) {
        console.error(error)
      }
    },
    async rekeyData(key) {
      try {
        await rekeyData(key.id)
        alert('Data re-keying started.')
      } catch (error) {
        console.error(error)
      }
    }
  }
}
</script>

<style scoped>
.e2e-encryption-container {
  display: flex;
  flex-wrap: wrap;
  gap: 20px;
}
.keys-container {
  flex: 1 1 400px;
}
table {
  width: 100%;
  border-collapse: collapse;
}
th, td {
  border: 1px solid #ccc;
  padding: 8px;
}
</style>
