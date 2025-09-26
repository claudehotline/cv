import { createRouter, createWebHistory } from 'vue-router'
import VideoStreams from '@/views/VideoStreams.vue'

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: '/',
      redirect: '/video-streams'
    },
    {
      path: '/video-streams',
      name: 'VideoStreams',
      component: VideoStreams
    },
    {
      path: '/analysis-results',
      name: 'AnalysisResults',
      component: () => import('@/views/AnalysisResults.vue')
    },
    {
      path: '/settings',
      name: 'Settings',
      component: () => import('@/views/Settings.vue')
    },
    {
      path: '/jpeg-test',
      name: 'JpegTest',
      component: () => import('@/views/JpegVideoTest.vue')
    }
  ]
})

export default router