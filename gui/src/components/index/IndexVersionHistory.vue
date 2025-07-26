<template>
  <div>
    <h2>Index Version History</h2>
    <table>
      <thead>
        <tr>
          <th>Version</th>
          <th>Creation Date</th>
          <th>Data Source</th>
          <th>Hash</th>
          <th>Actions</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="version in versions" :key="version.id">
          <td>{{ version.version }}</td>
          <td>{{ version.creationDate }}</td>
          <td>{{ version.dataSource }}</td>
          <td>{{ version.hash }}</td>
          <td>
            <button @click="viewDetails(version)">Details</button>
            <button @click="compareVersion(version)">Compare</button>
            <button @click="rollbackToVersion(version)">Rollback</button>
          </td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<script>
export default {
  props: {
    versions: {
      type: Array,
      required: true
    }
  },
  methods: {
    viewDetails(version) {
      alert(`Viewing details for version ${version.version}`)
    },
    compareVersion(version) {
      alert(`Comparing version ${version.version}`)
    },
    rollbackToVersion(version) {
      alert(`Rolling back to version ${version.version}`)
    }
  }
}
</script>

<style scoped>
table {
  width: 100%;
  border-collapse: collapse;
}
th, td {
  border: 1px solid #ccc;
  padding: 8px;
}
</style>
