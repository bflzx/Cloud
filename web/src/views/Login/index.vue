<script setup>
    import { reactive, watch} from 'vue';
    import request from '../../util/request'
    const user = reactive({
        username:'',
        password:''
    })

    const onLogin = async =>{
        const res = await request.post('/login',{
            username:user.username,
            password:user.password
        })
        console.log(res)
    }
</script>

<template>
    <div id="screen">
        <div class="login">
            <div class="login-header">
                <h1 class="logintitle">登录</h1>
            </div>

            <div class="login-form">
                <el-form :model="user" label-width="auto">
                    <el-form-item label="账号">
                        <el-input v-model="user.username"></el-input>
                    </el-form-item>

                    <el-form-item label="密码">
                        <el-input v-model="user.password"></el-input>
                    </el-form-item>

                    <el-form-item class="submit-form">
                        <el-button class="submit-button" type="primary" @click="onLogin">登录</el-button>
                    </el-form-item>
                </el-form>
            </div>
        </div>
    </div>

</template>

<style scoped>
    #screen{
        position:fixed;
        display:flex;
        top:0;
        left:0;
        right:0;
        bottom:0;
        background-color: green;
    }
    .login{
        width:380px;
        align-self: center;
        margin:0 auto;
        background-color:red;
        display:flex;
        flex-direction: column;
    }
    .login-header{
        display:flex;
        justify-content: center;
    }
    .logintitle{
        margin:0;
    }
    .submit-button{
        margin:0 auto;
    }
</style>