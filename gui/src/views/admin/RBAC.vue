<template>
  <div>
    <h1>RBAC</h1>
    <div class="rbac-container">
      <div class="roles-container">
        <h2>Roles</h2>
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Description</th>
              <th>Actions</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="role in roles" :key="role.id">
              <td>{{ role.name }}</td>
              <td>{{ role.description }}</td>
              <td>
                <button @click="editRole(role)">Edit</button>
                <button @click="deleteRole(role)">Delete</button>
              </td>
            </tr>
          </tbody>
        </table>
        <form @submit.prevent="createRole">
          <h3>Create Role</h3>
          <input v-model="newRole.name" placeholder="Name" required />
          <input v-model="newRole.description" placeholder="Description" required />
          <button type="submit">Create</button>
        </form>
      </div>
      <div class="permissions-container">
        <h2>Permissions</h2>
        <table>
          <thead>
            <tr>
              <th>Name</th>
              <th>Description</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="permission in permissions" :key="permission.id">
              <td>{{ permission.name }}</td>
              <td>{{ permission.description }}</td>
            </tr>
          </tbody>
        </table>
      </div>
      <div class="role-permission-mapping-container">
        <h2>Role-Permission Mapping</h2>
        <table>
          <thead>
            <tr>
              <th>Role</th>
              <th v-for="permission in permissions" :key="permission.id">{{ permission.name }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="role in roles" :key="role.id">
              <td>{{ role.name }}</td>
              <td v-for="permission in permissions" :key="permission.id">
                <input type="checkbox" :checked="roleHasPermission(role, permission)" @change="toggleRolePermission(role, permission)" />
              </td>
            </tr>
          </tbody>
        </table>
      </div>
      <div class="user-role-mapping-container">
        <h2>User-Role Mapping</h2>
        <table>
          <thead>
            <tr>
              <th>User</th>
              <th v-for="role in roles" :key="role.id">{{ role.name }}</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="user in users" :key="user.id">
              <td>{{ user.name }}</td>
              <td v-for="role in roles" :key="role.id">
                <input type="checkbox" :checked="userHasRole(user, role)" @change="toggleUserRole(user, role)" />
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>
</template>

<script>
import {
  getRoles,
  createRole,
  updateRole,
  deleteRole,
  getPermissions,
  getRolePermissions,
  updateRolePermissions,
  getUsers,
  getUserRoles,
  updateUserRoles
} from '@/services/rbac'

export default {
  data() {
    return {
      roles: [],
      permissions: [],
      users: [],
      newRole: {
        name: '',
        description: ''
      },
      rolePermissions: {},
      userRoles: {}
    }
  },
  created() {
    this.fetchData()
  },
  methods: {
    async fetchData() {
      try {
        const [rolesRes, permissionsRes, usersRes] = await Promise.all([
          getRoles(),
          getPermissions(),
          getUsers()
        ])
        this.roles = rolesRes.data
        this.permissions = permissionsRes.data
        this.users = usersRes.data

        for (const role of this.roles) {
          const rolePermissionsRes = await getRolePermissions(role.id)
          this.rolePermissions[role.id] = rolePermissionsRes.data
        }

        for (const user of this.users) {
          const userRolesRes = await getUserRoles(user.id)
          this.userRoles[user.id] = userRolesRes.data
        }
      } catch (error) {
        console.error(error)
      }
    },
    async createRole() {
      try {
        const res = await createRole(this.newRole)
        this.roles.push(res.data)
        this.newRole.name = ''
        this.newRole.description = ''
      } catch (error) {
        console.error(error)
      }
    },
    async editRole(role) {
      // Not implemented
    },
    async deleteRole(role) {
      try {
        await deleteRole(role.id)
        this.roles = this.roles.filter(r => r.id !== role.id)
      } catch (error) {
        console.error(error)
      }
    },
    roleHasPermission(role, permission) {
      return this.rolePermissions[role.id]?.includes(permission.id)
    },
    async toggleRolePermission(role, permission) {
      try {
        const permissions = this.rolePermissions[role.id] || []
        const index = permissions.indexOf(permission.id)
        if (index > -1) {
          permissions.splice(index, 1)
        } else {
          permissions.push(permission.id)
        }
        await updateRolePermissions(role.id, { permissions })
      } catch (error) {
        console.error(error)
      }
    },
    userHasRole(user, role) {
      return this.userRoles[user.id]?.includes(role.id)
    },
    async toggleUserRole(user, role) {
      try {
        const roles = this.userRoles[user.id] || []
        const index = roles.indexOf(role.id)
        if (index > -1) {
          roles.splice(index, 1)
        } else {
          roles.push(role.id)
        }
        await updateUserRoles(user.id, { roles })
      } catch (error) {
        console.error(error)
      }
    }
  }
}
</script>

<style scoped>
.rbac-container {
  display: flex;
  flex-wrap: wrap;
  gap: 20px;
}
.roles-container,
.permissions-container,
.role-permission-mapping-container,
.user-role-mapping-container {
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
