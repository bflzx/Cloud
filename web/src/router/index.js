import { createRouter, createWebHistory } from 'vue-router';

const routes = [
    {
        path:'/',
        redirect:'/login'
    },
    {
        path:'/login',
        name:'Login',
        component: () => import('../views/Login/index.vue'),
        meta: { requiresAuth: false},
    },
    {
        path:'/register',
        name:'Register',
        component: () => import('../views/Register/index.vue'),
        meta: { requiresAuth : false}
    },
    {
        path:'/main',
        name:'Main',
        component: () => import('../views/Main/index.vue'),
        meta: { requiresAuth : true}
    }
]

const router = createRouter({
    history:createWebHistory(),
    routes
})

router.beforeEach((to,from,next) => {
    const token = localStorage.getItem('token')
    if(to.meta.requiresAuth != false && !token){
        next('/login')
    }else if(to.path == '/login' && token){
        next('/main')
    }else{
        next()
    }
})


export default router;