import axios from 'axios'

const request = axios.create({
    baseURL: 'http://192.168.195.129:8080/api',
    timeout: 10000,
})

// ==================== 请求拦截器 ====================
request.interceptors.request.use(
    (config) => {
        // 1. 统一携带 token（登录后存到 localStorage）
        const token = localStorage.getItem('token')
        if (token) {
            config.headers.Authorization = `Bearer ${token}`
        }

        // 2. GET 请求的时间戳，防止 IE 缓存
        if (config.method === 'get') {
            config.params = {
                ...config.params,
                _t: Date.now(),
            }
        }

        return config
    },
    (error) => Promise.reject(error)
)

// ==================== 响应拦截器 ====================
request.interceptors.response.use(
    (response) => {
        // 和后端约定统一返回格式: { code: 0, data: ..., message: 'ok' }
        const res = response.data

        if (res.code && res.code !== 0) {
            // 业务错误（比如账号密码错误）
            console.error(res.message || '请求失败')
            return Promise.reject(new Error(res.message || '请求失败'))
        }

        return res // 调用方直接拿到 { code, data, message }
    },
    (error) => {
        // HTTP 层面的错误（网络断开、超时、状态码非 2xx）
        const status = error.response?.status
        let message = '网络异常'

        switch (status) {
            case 400: message = '请求参数错误'; break
            case 401: message = '未登录或登录已过期'; break      // 可以跳回登录页
            case 403: message = '没有操作权限'; break
            case 404: message = '请求的资源不存在'; break
            case 500: message = '服务器内部错误'; break
        }

        console.error(message)
        return Promise.reject(new Error(message))
    }
)

export default request