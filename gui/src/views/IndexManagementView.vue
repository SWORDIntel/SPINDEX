<template>
  <div class="index-management-view">
    <h2>Index Management</h2>
    <button @click="showCreateIndexWizard = true">Create New Index</button>
    <table id="indexes-table">
      <thead>
        <tr>
          <th>ID</th>
          <th>Name</th>
          <th>Type</th>
          <th>Status</th>
          <th>Version</th>
          <th>Created At</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="index in indexes" :key="index.id">
          <td>{{ index.id }}</td>
          <td>{{ index.name }}</td>
          <td>{{ index.type }}</td>
          <td>{{ index.status }}</td>
          <td>{{ index.version }}</td>
          <td>{{ index.createdAt }}</td>
          <td>
            <button @click="viewIndexDetails(index)">Details</button>
            <button @click="deleteIndex(index)">Delete</button>
          </td>
        </tr>
      </tbody>
    </table>
    <div v-if="selectedIndex" class="modal">
      <div class="modal-content">
        <span class="close" @click="selectedIndex = null">&times;</span>
        <h4>Index Details (for Index ID: {{ selectedIndex.id }})</h4>
        <pre>{{ selectedIndex }}</pre>
        <IndexVersionHistory :versions="selectedIndex.versions" />
      </div>
    </div>
    <CreateIndexWizard v-if="showCreateIndexWizard" @close="showCreateIndexWizard = false" />
  </div>
</template>

<script>
import IndexVersionHistory from '@/components/index/IndexVersionHistory.vue'
import CreateIndexWizard from '@/components/index/CreateIndexWizard.vue'

export default {
  components: {
    IndexVersionHistory,
    CreateIndexWizard
  },
  data() {
    return {
      indexes: [],
      selectedIndex: null,
      showCreateIndexWizard: false
    }
  },
  methods: {
    viewIndexDetails(index) {
      this.selectedIndex = index
    },
    deleteIndex(index) {
      alert(`Deleting index ${index.name}`)
    }
  }
}
</script>

<style scoped>
.index-management-view {
  padding: 20px;
}
.modal {
  display: block;
  position: fixed;
  z-index: 1;
  left: 0;
  top: 0;
  width: 100%;
  height: 100%;
  overflow: auto;
  background-color: rgba(0,0,0,0.4);
}
.modal-content {
  background-color: #fefefe;
  margin: 15% auto;
  padding: 20px;
  border: 1px solid #888;
  width: 80%;
}
.close {
  color: #aaa;
  float: right;
  font-size: 28px;
  font-weight: bold;
}
.close:hover,
.close:focus {
  color: black;
  text-decoration: none;
  cursor: pointer;
}
</style>
