import { createRouter, createWebHistory } from 'vue-router';
import Login from '../views/Login/index.vue'

const routes = [
    {
        path:'/',
        redirect:'/login'
    },
    {
        path:'/login',
        name:'Login',
        component: Login,
        meta: { requiresAuth: false},
    },
]

const router = createRouter({
    history:createWebHistory(),
    routes
})

export default router;