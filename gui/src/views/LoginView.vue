<template>
  <div class="login-view">
    <h2>Login</h2>
    <form @submit.prevent="handleLogin">
      <div>
        <label for="username">Username:</label>
        <input type="text" id="username" v-model="username" />
      </div>
      <div>
        <label for="password">Password:</label>
        <input type="password" id="password" v-model="password" />
      </div>
      <button type="submit">Login</button>
      <p v-if="errorMessage" class="error">{{ errorMessage }}</p>
    </form>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'
import { useRouter } from 'vue-router'
// import { useAuthStore } from '@/store/auth' // Example

const username = ref('')
const password = ref('')
const errorMessage = ref('')
const router = useRouter()
// const authStore = useAuthStore()

const handleLogin = async () => {
  errorMessage.value = ''
  // Placeholder login logic
  // try {
  //   await authStore.login({ username: username.value, password: password.value })
  //   const redirectPath = router.currentRoute.value.query.redirect || '/'
  //   router.push(redirectPath as string)
  // } catch (error) {
  //   errorMessage.value = (error as Error).message || 'Failed to login.'
  // }
  if (username.value === 'admin' && password.value === 'password') {
    alert('Mock login successful!')
    router.push('/')
  } else {
    errorMessage.value = 'Invalid credentials (mock login).'
  }
}
</script>

<style scoped>
.login-view {
  max-width: 400px;
  margin: 50px auto;
  padding: 20px;
  border: 1px solid #ccc;
  border-radius: 8px;
  box-shadow: 0 2px 10px rgba(0,0,0,0.1);
}
.login-view div {
  margin-bottom: 15px;
}
.login-view label {
  display: block;
  margin-bottom: 5px;
}
.login-view input {
  width: calc(100% - 20px);
  padding: 8px 10px;
  border: 1px solid #ddd;
  border-radius: 4px;
}
.login-view button {
  padding: 10px 15px;
  background-color: var(--spindex-primary, #1a73e8);
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}
.login-view button:hover {
  opacity: 0.9;
}
.error {
  color: var(--spindex-danger, #dc3545);
  margin-top: 10px;
}
</style>
