<script setup>
    import { reactive } from 'vue';
    import { useRouter } from 'vue-router'
    import request from '../../util/request'
    const router = useRouter();
    const user = reactive({
        username:'',
        password:''
    })

    const onRegister = async () =>{
        try{
            const res = await request.post('/register',{
                username: user.username,
                password: user.password    
            }
            )
            router.push('/login');
        } catch(err){
            console.log(err.message);
        }
    }
</script>

<template>
    <div id="screen">
        <div class="login">
            <div class="login-header">
                <h1 class="logintitle">注册</h1>
            </div>

            <div class="login-form">
                <el-form :model="user" label-width="80px">
                    <el-form-item label="账号">
                        <el-input v-model="user.username"></el-input>
                    </el-form-item>

                    <el-form-item label="密码">
                        <el-input v-model="user.password"></el-input>
                    </el-form-item>

                    <el-form-item class="submit-form">
                        <el-button class="register-button" type="warning" @click="onRegister">注册</el-button>
                    </el-form-item>
                </el-form>
            </div>
        </div>
    </div>

</template>

<style scoped>

    #screen {
    position: fixed;
    inset: 0;
    display: flex;
    justify-content: center;
    align-items: center;
    background-color: green;
    }

    .login {
        width: 380px;
        background-color: red;
        padding: 32px 24px;
        border-radius: 8px;
    }

    .login-header {
        display: flex;
        justify-content: center;
        margin-bottom: 24px;
    }

    .logintitle {
        margin: 0;
    }

    .submit-form {
        display: flex;
        justify-content: center;
        gap: 12px;
    }
</style>