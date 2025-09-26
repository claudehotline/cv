<template>
  <div id="app">
    <h1>简单测试页面</h1>
    <p>如果您看到这个内容，说明Vue正在工作。</p>
    <button @click="testClick">点击测试</button>
    <p v-if="clicked">按钮点击成功！</p>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue'

const clicked = ref(false)

const testClick = () => {
  clicked.value = true
  console.log('按钮被点击了')
}

// 在组件挂载时输出信息
console.log('简单测试组件已加载')
</script>

<style scoped>
#app {
  padding: 20px;
  font-family: Arial, sans-serif;
}

h1 {
  color: #2c3e50;
}

button {
  padding: 10px 20px;
  background-color: #409eff;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

button:hover {
  background-color: #66b1ff;
}
</style>