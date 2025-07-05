<template>
  <header class="app-header">
    <div class="logo-container">
      <!-- Placeholder for logo -->
      <img src="@/assets/logo_placeholder.png" alt="SPINDEX Logo" class="logo" @click="goToHome" />
      <span class="app-title">SPINDEX</span>
    </div>
    <nav class="navigation-links">
      <!-- Placeholder for main navigation links if needed in header -->
      <!-- <RouterLink to="/search">Search</RouterLink> -->
      <!-- <RouterLink to="/indexes">Indexes</RouterLink> -->
    </nav>
    <div class="user-menu-container">
      <!-- Placeholder for user information and menu -->
      <span v.if="isAuthenticated">Welcome, {{ userName }}</span>
      <button v-if="isAuthenticated" @click="logout">Logout</button>
      <button v-if="!isAuthenticated" @click="login">Login</button>
    </div>
  </header>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { useRouter } from 'vue-router'
// import { useAuthStore } from '@/store/auth' // Example: Assuming an auth store

// Prepare for future auth state
interface UserInfo {
  username: string;
  role: 'admin' | 'analyst' | 'viewer'; // Example roles
}

// const authStore = useAuthStore()
const router = useRouter()

// Placeholder computed properties - replace with actual store getters
// const currentUser = computed<UserInfo | null>(() => authStore.user)
const currentUser = computed<UserInfo | null>(() => null) // Placeholder

const isAuthenticated = computed(() => !!currentUser.value)
const userName = computed(() => currentUser.value?.username || 'Guest')

const goToHome = () => {
  router.push('/')
}

const login = () => {
  router.push('/login')
}

const logout = () => {
  // authStore.logout()
  // router.push('/login')
  alert('Logout action placeholder')
}
</script>

<style scoped>
.app-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 10px 20px;
  background-color: #2c3e50; /* Dark background for header */
  color: white;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.logo-container {
  display: flex;
  align-items: center;
  cursor: pointer;
}

.logo {
  height: 40px; /* Adjust as needed */
  margin-right: 10px;
  /* Basic placeholder style if image is missing */
  border: 1px solid #fff;
  background-color: #fff3; /* Light placeholder background */
}

.app-title {
  font-size: 1.5em;
  font-weight: bold;
}

.navigation-links a {
  color: white;
  margin: 0 10px;
  text-decoration: none;
}

.navigation-links a:hover {
  text-decoration: underline;
}

.user-menu-container span {
  margin-right: 15px;
}

.user-menu-container button {
  padding: 8px 12px;
  background-color: #42b983; /* Vue green for buttons */
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

.user-menu-container button:hover {
  background-color: #36a476;
}
</style>
