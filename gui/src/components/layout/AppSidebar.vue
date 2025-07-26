<template>
  <aside class="app-sidebar">
    <nav>
      <ul>
        <template v-for="item in navItems" :key="item.path">
          <!-- Top-level item -->
          <!-- TODO: Add RBAC v-if check here later using item.requiredRole and authStore -->
          <li>
            <RouterLink :to="item.path">
              <span v-if="item.icon" :class="item.icon"></span> <!-- Placeholder for icon -->
              {{ item.label }}
            </RouterLink>
            <!-- Child items if any -->
            <ul v-if="item.children && item.children.length > 0" class="child-menu">
              <template v-for="child in item.children" :key="child.path">
                <!-- TODO: Add RBAC v-if check here for child items -->
                <li>
                  <RouterLink :to="child.path">
                    <span v-if="child.icon" :class="child.icon"></span>
                    {{ child.label }}
                  </RouterLink>
                </li>
              </template>
            </ul>
          </li>
        </template>
      </ul>
    </nav>
  </aside>
</template>

<script setup lang="ts">
import { RouterLink } from 'vue-router'
import { ref } from 'vue'

interface NavItem {
  path: string;
  label: string;
  icon?: string; // e.g., an SVG path or a class name for an icon font
  requiredRole?: Array<'admin' | 'analyst' | 'viewer'>; // Example roles
  children?: NavItem[];
}

// Placeholder navigation items. In a real app, this might come from a store or config.
// For now, no roles are required for these basic links.
const navItems = ref<NavItem[]>([
  { path: '/', label: 'Dashboard', icon: 'icon-dashboard' },
  { path: '/search', label: 'Search', icon: 'icon-search' },
  { path: '/indexes', label: 'Index Management', icon: 'icon-database' },
  { path: '/history', label: 'Query History', icon: 'icon-history' },
  {
    path: '/admin',
    label: 'Administration',
    icon: 'icon-admin',
    requiredRole: ['admin'], // Example admin-only section
    children: [
      { path: '/admin/users', label: 'User Management', icon: 'icon-users', requiredRole: ['admin'] },
      { path: '/admin/system', label: 'System Status', icon: 'icon-system', requiredRole: ['admin'] },
      { path: '/admin/rbac', label: 'RBAC', icon: 'icon-rbac', requiredRole: ['admin'] },
    ]
  },
  { path: '/settings', label: 'Settings', icon: 'icon-settings' },
])

// Future: Filter navItems based on user role from authStore
// const filteredNavItems = computed(() => {
//   const userRoles = authStore.user?.roles || []; // Assuming roles is an array of strings
//   return navItems.value.filter(item =>
//     !item.requiredRole || item.requiredRole.some(role => userRoles.includes(role))
//   ).map(item => {
//     if (item.children) {
//       return {
//         ...item,
//         children: item.children.filter(child =>
//           !child.requiredRole || child.requiredRole.some(role => userRoles.includes(role))
//         )
//       };
//     }
//     return item;
//   });
// });

</script>

<style scoped>
/* Styles for icons can be added here if using classes */
/* .icon-dashboard::before { content: '🏠'; margin-right: 8px; } */
/* .icon-search::before { content: '🔍'; margin-right: 8px; } */
/* ... etc. ... */

.app-sidebar {
  width: 220px; /* Adjust width as needed */
  background-color: #34495e; /* Slightly lighter dark shade for sidebar */
  color: white;
  padding: 20px 0;
  height: 100%; /* Fill height of its container */
  box-shadow: 2px 0 5px rgba(0,0,0,0.1);
  flex-shrink: 0; /* Prevent sidebar from shrinking */
}

nav ul {
  list-style-type: none;
  padding: 0;
  margin: 0;
}

nav li a {
  display: block;
  color: #ecf0f1; /* Lighter text color for links */
  padding: 12px 20px;
  text-decoration: none;
  transition: background-color 0.2s ease-in-out;
}

nav li a:hover,
nav li a.router-link-exact-active { /* Style for active link */
  background-color: #4a627a; /* Highlight color */
  color: #ffffff;
  font-weight: bold;
}
</style>
