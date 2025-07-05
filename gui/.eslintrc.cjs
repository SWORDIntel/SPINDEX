/* eslint-env node */
require('@rushstack/eslint-patch/modern-module-resolution');

module.exports = {
  root: true,
  env: {
    browser: true,
    es2021: true,
    node: true, // For config files and scripts if any
  },
  extends: [
    'plugin:vue/vue3-essential', // Base Vue 3 rules
    'eslint:recommended', // Basic ESLint recommendations
    '@vue/eslint-config-typescript/recommended', // TypeScript rules for Vue
    'eslint-config-prettier' // Turns off ESLint rules that conflict with Prettier
  ],
  parserOptions: {
    ecmaVersion: 'latest',
    parser: '@typescript-eslint/parser', // Use TypeScript parser
    sourceType: 'module',
  },
  plugins: [
    'vue', // Enables Vue linting
    '@typescript-eslint' // Enables TypeScript linting
  ],
  rules: {
    // Basic ESLint rules
    'no-console': process.env.NODE_ENV === 'production' ? 'warn' : 'off',
    'no-debugger': process.env.NODE_ENV === 'production' ? 'warn' : 'off',
    'no-unused-vars': 'off', // Handled by @typescript-eslint/no-unused-vars

    // TypeScript specific rules
    '@typescript-eslint/no-unused-vars': ['warn', { argsIgnorePattern: '^_' }],
    '@typescript-eslint/explicit-module-boundary-types': 'off', // Can be enabled for stricter typing
    '@typescript-eslint/no-explicit-any': 'warn', // Warn instead of error for 'any' type

    // Vue specific rules (examples, customize as needed)
    'vue/multi-word-component-names': 'warn', // Recommend multi-word, but warn for now
    'vue/no-unused-vars': 'warn', // For template variable usage
    'vue/no-v-html': 'warn', // Discourage v-html unless necessary
    'vue/component-name-in-template-casing': ['error', 'PascalCase'],
    'vue/html-self-closing': ['error', {
      'html': {
        'void': 'always',
        'normal': 'always',
        'component': 'always'
      },
      'svg': 'always',
      'math': 'always'
    }],

    // Prettier compatibility (eslint-config-prettier should handle most)
    // 'prettier/prettier': 'warn', // If using eslint-plugin-prettier (optional)

    // Add your custom rules here
  },
  globals: {
    defineProps: 'readonly',
    defineEmits: 'readonly',
    defineExpose: 'readonly',
    withDefaults: 'readonly'
  }
};
